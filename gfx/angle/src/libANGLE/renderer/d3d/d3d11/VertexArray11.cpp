//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArray11:
//   Implementation of rx::VertexArray11.
//

#include "libANGLE/renderer/d3d/d3d11/VertexArray11.h"

#include "common/bitset_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Context11.h"

using namespace angle;

namespace rx
{

VertexArray11::VertexArray11(const gl::VertexArrayState &data)
    : VertexArrayImpl(data),
      mAttributeStorageTypes(data.getMaxAttribs(), VertexStorageType::CURRENT_VALUE),
      mTranslatedAttribs(data.getMaxAttribs()),
      mCurrentBuffers(data.getMaxAttribs()),
      mAppliedNumViewsToDivisor(1)
{
    for (size_t attribIndex = 0; attribIndex < mCurrentBuffers.size(); ++attribIndex)
    {
        mOnBufferDataDirty.emplace_back(this, attribIndex);
    }
}

void VertexArray11::destroy(const gl::Context *context)
{
    for (auto &buffer : mCurrentBuffers)
    {
        if (buffer.get())
        {
            buffer.set(context, nullptr);
        }
    }
}

void VertexArray11::syncState(const gl::Context *context,
                              const gl::VertexArray::DirtyBits &dirtyBits)
{
    ASSERT(dirtyBits.any());

    // Generate a state serial. This serial is used in the program class to validate the cached
    // input layout, and skip recomputation in the fast path.
    auto renderer       = GetImplAs<Context11>(context)->getRenderer();
    mCurrentStateSerial = renderer->generateSerial();

    // TODO(jmadill): Individual attribute invalidation.
    renderer->getStateManager()->invalidateVertexBuffer();

    for (auto dirtyBit : dirtyBits)
    {
        if (dirtyBit == gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER)
            continue;

        size_t index = gl::VertexArray::GetVertexIndexFromDirtyBit(dirtyBit);
        // TODO(jiawei.shao@intel.com): Vertex Attrib Bindings
        ASSERT(index == mData.getBindingIndexFromAttribIndex(index));
        mAttribsToUpdate.set(index);
    }
}

bool VertexArray11::flushAttribUpdates(const gl::Context *context)
{
    const gl::Program *program  = context->getGLState().getProgram();
    const auto &activeLocations = program->getActiveAttribLocationsMask();

    if (mAttribsToUpdate.any())
    {
        // Skip attrib locations the program doesn't use.
        gl::AttributesMask activeToUpdate = mAttribsToUpdate & activeLocations;

        for (auto toUpdateIndex : activeToUpdate)
        {
            mAttribsToUpdate.reset(toUpdateIndex);
            updateVertexAttribStorage(context, toUpdateIndex);
        }

        return true;
    }

    return false;
}

void VertexArray11::updateVertexAttribStorage(const gl::Context *context, size_t attribIndex)
{
    const auto &attrib = mData.getVertexAttribute(attribIndex);
    const auto &binding = mData.getBindingFromAttribIndex(attribIndex);

    // Note: having an unchanged storage type doesn't mean the attribute is clean.
    auto oldStorageType = mAttributeStorageTypes[attribIndex];
    auto newStorageType = ClassifyAttributeStorage(attrib, binding);

    mAttributeStorageTypes[attribIndex] = newStorageType;

    StateManager11 *stateManager = GetImplAs<Context11>(context)->getRenderer()->getStateManager();

    if (newStorageType == VertexStorageType::DYNAMIC)
    {
        if (oldStorageType != VertexStorageType::DYNAMIC)
        {
            // Sync dynamic attribs in a different set.
            mAttribsToTranslate.reset(attribIndex);
            mDynamicAttribsMask.set(attribIndex);
        }
    }
    else
    {
        mAttribsToTranslate.set(attribIndex);
        stateManager->invalidateVertexAttributeTranslation();

        if (oldStorageType == VertexStorageType::DYNAMIC)
        {
            ASSERT(mDynamicAttribsMask[attribIndex]);
            mDynamicAttribsMask.reset(attribIndex);
        }
    }

    gl::Buffer *oldBufferGL = mCurrentBuffers[attribIndex].get();
    gl::Buffer *newBufferGL = binding.getBuffer().get();
    Buffer11 *oldBuffer11   = oldBufferGL ? GetImplAs<Buffer11>(oldBufferGL) : nullptr;
    Buffer11 *newBuffer11   = newBufferGL ? GetImplAs<Buffer11>(newBufferGL) : nullptr;

    if (oldBuffer11 != newBuffer11 || oldStorageType != newStorageType)
    {
        OnBufferDataDirtyChannel *newChannel = nullptr;

        if (newStorageType == VertexStorageType::CURRENT_VALUE)
        {
            stateManager->invalidateCurrentValueAttrib(attribIndex);
        }
        else if (newBuffer11 != nullptr)
        {
            // Note that for static callbacks, promotion to a static buffer from a dynamic buffer
            // means we need to tag dynamic buffers with static callbacks.
            switch (newStorageType)
            {
                case VertexStorageType::DIRECT:
                    newChannel = newBuffer11->getDirectBroadcastChannel();
                    break;
                case VertexStorageType::STATIC:
                case VertexStorageType::DYNAMIC:
                    newChannel = newBuffer11->getStaticBroadcastChannel();
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
        }

        mOnBufferDataDirty[attribIndex].bind(newChannel);
        mCurrentBuffers[attribIndex].set(context, binding.getBuffer().get());
    }
}

bool VertexArray11::hasDynamicAttrib(const gl::Context *context)
{
    flushAttribUpdates(context);
    return mDynamicAttribsMask.any();
}

bool VertexArray11::hasDirtyOrDynamicAttrib(const gl::Context *context)
{
    flushAttribUpdates(context);
    return mAttribsToTranslate.any() || mDynamicAttribsMask.any();
}

gl::Error VertexArray11::updateDirtyAndDynamicAttribs(const gl::Context *context,
                                                      VertexDataManager *vertexDataManager,
                                                      GLint start,
                                                      GLsizei count,
                                                      GLsizei instances)
{
    flushAttribUpdates(context);

    const auto &glState         = context->getGLState();
    const gl::Program *program  = glState.getProgram();
    const auto &activeLocations = program->getActiveAttribLocationsMask();
    const auto &attribs         = mData.getVertexAttributes();
    const auto &bindings        = mData.getVertexBindings();
    mAppliedNumViewsToDivisor =
        (program != nullptr && program->usesMultiview()) ? program->getNumViews() : 1;

    if (mAttribsToTranslate.any())
    {
        // Skip attrib locations the program doesn't use, saving for the next frame.
        gl::AttributesMask dirtyActiveAttribs = (mAttribsToTranslate & activeLocations);

        for (auto dirtyAttribIndex : dirtyActiveAttribs)
        {
            mAttribsToTranslate.reset(dirtyAttribIndex);

            auto *translatedAttrib = &mTranslatedAttribs[dirtyAttribIndex];
            const auto &currentValue = glState.getVertexAttribCurrentValue(dirtyAttribIndex);

            // Record basic attrib info
            translatedAttrib->attribute = &attribs[dirtyAttribIndex];
            translatedAttrib->binding   = &bindings[translatedAttrib->attribute->bindingIndex];
            translatedAttrib->currentValueType = currentValue.Type;
            translatedAttrib->divisor =
                translatedAttrib->binding->getDivisor() * mAppliedNumViewsToDivisor;

            switch (mAttributeStorageTypes[dirtyAttribIndex])
            {
                case VertexStorageType::DIRECT:
                    VertexDataManager::StoreDirectAttrib(translatedAttrib);
                    break;
                case VertexStorageType::STATIC:
                {
                    ANGLE_TRY(VertexDataManager::StoreStaticAttrib(context, translatedAttrib));
                    break;
                }
                case VertexStorageType::CURRENT_VALUE:
                    // Current value attribs are managed by the StateManager11.
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
        }
    }

    if (mDynamicAttribsMask.any())
    {
        auto activeDynamicAttribs = (mDynamicAttribsMask & activeLocations);

        for (auto dynamicAttribIndex : activeDynamicAttribs)
        {
            auto *dynamicAttrib = &mTranslatedAttribs[dynamicAttribIndex];
            const auto &currentValue = glState.getVertexAttribCurrentValue(dynamicAttribIndex);

            // Record basic attrib info
            dynamicAttrib->attribute        = &attribs[dynamicAttribIndex];
            dynamicAttrib->binding          = &bindings[dynamicAttrib->attribute->bindingIndex];
            dynamicAttrib->currentValueType = currentValue.Type;
            dynamicAttrib->divisor =
                dynamicAttrib->binding->getDivisor() * mAppliedNumViewsToDivisor;
        }

        ANGLE_TRY(vertexDataManager->storeDynamicAttribs(
            context, &mTranslatedAttribs, activeDynamicAttribs, start, count, instances));
    }

    return gl::NoError();
}

const std::vector<TranslatedAttribute> &VertexArray11::getTranslatedAttribs() const
{
    return mTranslatedAttribs;
}

void VertexArray11::signal(size_t channelID, const gl::Context *context)
{
    ASSERT(mAttributeStorageTypes[channelID] != VertexStorageType::CURRENT_VALUE);

    // This can change a buffer's storage, we'll need to re-check.
    mAttribsToUpdate.set(channelID);

    // Changing the vertex attribute state can affect the vertex shader.
    Renderer11 *renderer = GetImplAs<Context11>(context)->getRenderer();
    renderer->getStateManager()->invalidateShaders();
}

void VertexArray11::clearDirtyAndPromoteDynamicAttribs(const gl::Context *context, GLsizei count)
{
    const gl::State &state      = context->getGLState();
    const gl::Program *program  = state.getProgram();
    const auto &activeLocations = program->getActiveAttribLocationsMask();
    mAttribsToUpdate &= ~activeLocations;

    // Promote to static after we clear the dirty attributes, otherwise we can lose dirtyness.
    auto activeDynamicAttribs = (mDynamicAttribsMask & activeLocations);
    VertexDataManager::PromoteDynamicAttribs(context, mTranslatedAttribs, activeDynamicAttribs,
                                             count);
}

void VertexArray11::markAllAttributeDivisorsForAdjustment(int numViews)
{
    if (mAppliedNumViewsToDivisor != numViews)
    {
        mAppliedNumViewsToDivisor = numViews;
        mAttribsToUpdate.set();
    }
}

}  // namespace rx

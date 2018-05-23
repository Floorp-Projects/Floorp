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
#include "libANGLE/renderer/d3d/IndexBuffer.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Context11.h"

using namespace angle;

namespace rx
{
VertexArray11::VertexArray11(const gl::VertexArrayState &data)
    : VertexArrayImpl(data),
      mAttributeStorageTypes(data.getMaxAttribs(), VertexStorageType::CURRENT_VALUE),
      mTranslatedAttribs(data.getMaxAttribs()),
      mAppliedNumViewsToDivisor(1),
      mCurrentElementArrayStorage(IndexStorageType::Invalid),
      mCachedDestinationIndexType(GL_NONE)
{
}

VertexArray11::~VertexArray11()
{
}

void VertexArray11::destroy(const gl::Context *context)
{
}

gl::Error VertexArray11::syncState(const gl::Context *context,
                                   const gl::VertexArray::DirtyBits &dirtyBits,
                                   const gl::VertexArray::DirtyAttribBitsArray &attribBits,
                                   const gl::VertexArray::DirtyBindingBitsArray &bindingBits)
{
    ASSERT(dirtyBits.any());

    Renderer11 *renderer         = GetImplAs<Context11>(context)->getRenderer();
    StateManager11 *stateManager = renderer->getStateManager();

    // Generate a state serial. This serial is used in the program class to validate the cached
    // input layout, and skip recomputation in the fast path.
    mCurrentStateSerial = renderer->generateSerial();

    bool invalidateVertexBuffer = false;

    // Make sure we trigger re-translation for static index or vertex data.
    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER:
            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER_DATA:
            {
                mLastDrawElementsType.reset();
                mLastDrawElementsIndices.reset();
                mLastPrimitiveRestartEnabled.reset();
                mCachedIndexInfo.reset();
                break;
            }

            default:
            {
                size_t index = gl::VertexArray::GetVertexIndexFromDirtyBit(dirtyBit);

                // TODO(jiawei.shao@intel.com): Vertex Attrib Bindings
                ASSERT(index == mState.getBindingIndexFromAttribIndex(index));

                if (dirtyBit < gl::VertexArray::DIRTY_BIT_BINDING_MAX)
                {
                    updateVertexAttribStorage(stateManager, dirtyBit, index);
                }
                else
                {
                    ASSERT(dirtyBit >= gl::VertexArray::DIRTY_BIT_BUFFER_DATA_0 &&
                           dirtyBit < gl::VertexArray::DIRTY_BIT_BUFFER_DATA_MAX);
                    if (mAttributeStorageTypes[index] == VertexStorageType::STATIC)
                    {
                        mAttribsToTranslate.set(index);
                    }
                }
                invalidateVertexBuffer = true;
                break;
            }
        }
    }

    if (invalidateVertexBuffer)
    {
        // TODO(jmadill): Individual attribute invalidation.
        stateManager->invalidateVertexBuffer();
    }

    return gl::NoError();
}

gl::Error VertexArray11::syncStateForDraw(const gl::Context *context,
                                          const gl::DrawCallParams &drawCallParams)
{
    Renderer11 *renderer         = GetImplAs<Context11>(context)->getRenderer();
    StateManager11 *stateManager = renderer->getStateManager();

    const gl::State &glState   = context->getGLState();
    const gl::Program *program = glState.getProgram();
    ASSERT(program);
    mAppliedNumViewsToDivisor = (program->usesMultiview() ? program->getNumViews() : 1);

    if (mAttribsToTranslate.any())
    {
        const gl::AttributesMask &activeLocations =
            glState.getProgram()->getActiveAttribLocationsMask();
        gl::AttributesMask activeDirtyAttribs = (mAttribsToTranslate & activeLocations);
        if (activeDirtyAttribs.any())
        {
            ANGLE_TRY(updateDirtyAttribs(context, activeDirtyAttribs));
            stateManager->invalidateInputLayout();
        }
    }

    if (mDynamicAttribsMask.any())
    {
        const gl::AttributesMask &activeLocations =
            glState.getProgram()->getActiveAttribLocationsMask();
        gl::AttributesMask activeDynamicAttribs = (mDynamicAttribsMask & activeLocations);

        if (activeDynamicAttribs.any())
        {
            ANGLE_TRY(updateDynamicAttribs(context, stateManager->getVertexDataManager(),
                                           drawCallParams, activeDynamicAttribs));
            stateManager->invalidateInputLayout();
        }
    }

    if (drawCallParams.isDrawElements())
    {
        bool restartEnabled = context->getGLState().isPrimitiveRestartEnabled();
        if (!mLastDrawElementsType.valid() ||
            mLastDrawElementsType.value() != drawCallParams.type() ||
            mLastDrawElementsIndices.value() != drawCallParams.indices() ||
            mLastPrimitiveRestartEnabled.value() != restartEnabled)
        {
            mLastDrawElementsType        = drawCallParams.type();
            mLastDrawElementsIndices     = drawCallParams.indices();
            mLastPrimitiveRestartEnabled = restartEnabled;

            ANGLE_TRY(updateElementArrayStorage(context, drawCallParams, restartEnabled));
            stateManager->invalidateIndexBuffer();
        }
        else if (mCurrentElementArrayStorage == IndexStorageType::Dynamic)
        {
            stateManager->invalidateIndexBuffer();
        }
    }

    return gl::NoError();
}

gl::Error VertexArray11::updateElementArrayStorage(const gl::Context *context,
                                                   const gl::DrawCallParams &drawCallParams,
                                                   bool restartEnabled)
{
    bool usePrimitiveRestartWorkaround =
        UsePrimitiveRestartWorkaround(restartEnabled, drawCallParams.type());

    ANGLE_TRY(GetIndexTranslationDestType(context, drawCallParams, usePrimitiveRestartWorkaround,
                                          &mCachedDestinationIndexType));

    unsigned int offset =
        static_cast<unsigned int>(reinterpret_cast<uintptr_t>(drawCallParams.indices()));

    mCurrentElementArrayStorage =
        ClassifyIndexStorage(context->getGLState(), mState.getElementArrayBuffer().get(),
                             drawCallParams.type(), mCachedDestinationIndexType, offset);

    // Mark non-static buffers as irrelevant.
    bool isStatic = (mCurrentElementArrayStorage == IndexStorageType::Static);
    mRelevantDirtyBitsMask.set(gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER_DATA, isStatic);

    return gl::NoError();
}

void VertexArray11::updateVertexAttribStorage(StateManager11 *stateManager,
                                              size_t dirtyBit,
                                              size_t attribIndex)
{
    const gl::VertexAttribute &attrib = mState.getVertexAttribute(attribIndex);
    const gl::VertexBinding &binding  = mState.getBindingFromAttribIndex(attribIndex);

    VertexStorageType newStorageType = ClassifyAttributeStorage(attrib, binding);

    // Note: having an unchanged storage type doesn't mean the attribute is clean.
    mAttribsToTranslate.set(attribIndex, newStorageType != VertexStorageType::DYNAMIC);

    if (mAttributeStorageTypes[attribIndex] == newStorageType)
        return;

    mAttributeStorageTypes[attribIndex] = newStorageType;
    mDynamicAttribsMask.set(attribIndex, newStorageType == VertexStorageType::DYNAMIC);

    // Mark non-static buffers as irrelevant.
    size_t bufferDataDirtyBit = (gl::VertexArray::DIRTY_BIT_BUFFER_DATA_0 + attribIndex);
    mRelevantDirtyBitsMask.set(bufferDataDirtyBit, newStorageType == VertexStorageType::STATIC);

    if (newStorageType == VertexStorageType::CURRENT_VALUE)
    {
        stateManager->invalidateCurrentValueAttrib(attribIndex);
    }
}

bool VertexArray11::hasActiveDynamicAttrib(const gl::Context *context)
{
    const auto &activeLocations =
        context->getGLState().getProgram()->getActiveAttribLocationsMask();
    gl::AttributesMask activeDynamicAttribs = (mDynamicAttribsMask & activeLocations);
    return activeDynamicAttribs.any();
}

gl::Error VertexArray11::updateDirtyAttribs(const gl::Context *context,
                                            const gl::AttributesMask &activeDirtyAttribs)
{
    const auto &glState  = context->getGLState();
    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    for (size_t dirtyAttribIndex : activeDirtyAttribs)
    {
        mAttribsToTranslate.reset(dirtyAttribIndex);

        auto *translatedAttrib   = &mTranslatedAttribs[dirtyAttribIndex];
        const auto &currentValue = glState.getVertexAttribCurrentValue(dirtyAttribIndex);

        // Record basic attrib info
        translatedAttrib->attribute        = &attribs[dirtyAttribIndex];
        translatedAttrib->binding          = &bindings[translatedAttrib->attribute->bindingIndex];
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

    return gl::NoError();
}

gl::Error VertexArray11::updateDynamicAttribs(const gl::Context *context,
                                              VertexDataManager *vertexDataManager,
                                              const gl::DrawCallParams &drawCallParams,
                                              const gl::AttributesMask &activeDynamicAttribs)
{
    const auto &glState  = context->getGLState();
    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    ANGLE_TRY(drawCallParams.ensureIndexRangeResolved(context));

    for (size_t dynamicAttribIndex : activeDynamicAttribs)
    {
        auto *dynamicAttrib      = &mTranslatedAttribs[dynamicAttribIndex];
        const auto &currentValue = glState.getVertexAttribCurrentValue(dynamicAttribIndex);

        // Record basic attrib info
        dynamicAttrib->attribute        = &attribs[dynamicAttribIndex];
        dynamicAttrib->binding          = &bindings[dynamicAttrib->attribute->bindingIndex];
        dynamicAttrib->currentValueType = currentValue.Type;
        dynamicAttrib->divisor = dynamicAttrib->binding->getDivisor() * mAppliedNumViewsToDivisor;
    }

    ANGLE_TRY(vertexDataManager->storeDynamicAttribs(
        context, &mTranslatedAttribs, activeDynamicAttribs, drawCallParams.firstVertex(),
        drawCallParams.vertexCount(), drawCallParams.instances()));

    VertexDataManager::PromoteDynamicAttribs(context, mTranslatedAttribs, activeDynamicAttribs,
                                             drawCallParams.vertexCount());

    return gl::NoError();
}

const std::vector<TranslatedAttribute> &VertexArray11::getTranslatedAttribs() const
{
    return mTranslatedAttribs;
}

void VertexArray11::markAllAttributeDivisorsForAdjustment(int numViews)
{
    if (mAppliedNumViewsToDivisor != numViews)
    {
        mAppliedNumViewsToDivisor = numViews;
        mAttribsToTranslate.set();
    }
}

const TranslatedIndexData &VertexArray11::getCachedIndexInfo() const
{
    ASSERT(mCachedIndexInfo.valid());
    return mCachedIndexInfo.value();
}

void VertexArray11::updateCachedIndexInfo(const TranslatedIndexData &indexInfo)
{
    mCachedIndexInfo = indexInfo;
}

bool VertexArray11::isCachedIndexInfoValid() const
{
    return mCachedIndexInfo.valid();
}

GLenum VertexArray11::getCachedDestinationIndexType() const
{
    return mCachedDestinationIndexType;
}

}  // namespace rx

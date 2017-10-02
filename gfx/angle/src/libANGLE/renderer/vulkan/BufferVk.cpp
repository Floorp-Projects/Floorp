//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferVk.cpp:
//    Implements the class methods for BufferVk.
//

#include "libANGLE/renderer/vulkan/BufferVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

BufferVk::BufferVk(const gl::BufferState &state) : BufferImpl(state), mRequiredSize(0)
{
}

BufferVk::~BufferVk()
{
}

void BufferVk::destroy(const gl::Context *context)
{
    VkDevice device = GetImplAs<ContextVk>(context)->getDevice();

    mBuffer.destroy(device);
}

gl::Error BufferVk::setData(const gl::Context *context,
                            GLenum target,
                            const void *data,
                            size_t size,
                            GLenum usage)
{
    ContextVk *contextVk = GetImplAs<ContextVk>(context);
    auto device          = contextVk->getDevice();

    // TODO(jmadill): Proper usage bit implementation. Likely will involve multiple backing buffers
    // like in D3D11.
    VkBufferCreateInfo createInfo;
    createInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.pNext                 = nullptr;
    createInfo.flags                 = 0;
    createInfo.size                  = size;
    createInfo.usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices   = nullptr;

    vk::Buffer newBuffer;
    ANGLE_TRY(newBuffer.init(device, createInfo));

    // Find a compatible memory pool index. If the index doesn't change, we could cache it.
    // Not finding a valid memory pool means an out-of-spec driver, or internal error.
    // TODO(jmadill): More efficient memory allocation.
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, newBuffer.getHandle(), &memoryRequirements);

    // The requirements size is not always equal to the specified API size.
    ASSERT(memoryRequirements.size >= size);
    mRequiredSize = static_cast<size_t>(memoryRequirements.size);

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(contextVk->getRenderer()->getPhysicalDevice(),
                                        &memoryProperties);

    auto memoryTypeIndex =
        FindMemoryType(memoryProperties, memoryRequirements,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    ANGLE_VK_CHECK(memoryTypeIndex.valid(), VK_ERROR_INCOMPATIBLE_DRIVER);

    VkMemoryAllocateInfo allocInfo;
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext           = nullptr;
    allocInfo.memoryTypeIndex = memoryTypeIndex.value();
    allocInfo.allocationSize  = memoryRequirements.size;

    ANGLE_TRY(newBuffer.getMemory().allocate(device, allocInfo));
    ANGLE_TRY(newBuffer.bindMemory(device));

    mBuffer.retain(device, std::move(newBuffer));

    if (data)
    {
        ANGLE_TRY(setDataImpl(device, static_cast<const uint8_t *>(data), size, 0));
    }

    return gl::NoError();
}

gl::Error BufferVk::setSubData(const gl::Context *context,
                               GLenum target,
                               const void *data,
                               size_t size,
                               size_t offset)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBuffer.getMemory().getHandle() != VK_NULL_HANDLE);

    VkDevice device = GetImplAs<ContextVk>(context)->getDevice();

    ANGLE_TRY(setDataImpl(device, static_cast<const uint8_t *>(data), size, offset));

    return gl::NoError();
}

gl::Error BufferVk::copySubData(const gl::Context *context,
                                BufferImpl *source,
                                GLintptr sourceOffset,
                                GLintptr destOffset,
                                GLsizeiptr size)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error BufferVk::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBuffer.getMemory().getHandle() != VK_NULL_HANDLE);

    VkDevice device = GetImplAs<ContextVk>(context)->getDevice();

    ANGLE_TRY(mBuffer.getMemory().map(device, 0, mState.getSize(), 0,
                                      reinterpret_cast<uint8_t **>(mapPtr)));

    return gl::NoError();
}

gl::Error BufferVk::mapRange(const gl::Context *context,
                             size_t offset,
                             size_t length,
                             GLbitfield access,
                             void **mapPtr)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBuffer.getMemory().getHandle() != VK_NULL_HANDLE);

    VkDevice device = GetImplAs<ContextVk>(context)->getDevice();

    ANGLE_TRY(
        mBuffer.getMemory().map(device, offset, length, 0, reinterpret_cast<uint8_t **>(mapPtr)));

    return gl::NoError();
}

gl::Error BufferVk::unmap(const gl::Context *context, GLboolean *result)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBuffer.getMemory().getHandle() != VK_NULL_HANDLE);

    VkDevice device = GetImplAs<ContextVk>(context)->getDevice();

    mBuffer.getMemory().unmap(device);

    return gl::NoError();
}

gl::Error BufferVk::getIndexRange(const gl::Context *context,
                                  GLenum type,
                                  size_t offset,
                                  size_t count,
                                  bool primitiveRestartEnabled,
                                  gl::IndexRange *outRange)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

vk::Error BufferVk::setDataImpl(VkDevice device, const uint8_t *data, size_t size, size_t offset)
{
    uint8_t *mapPointer = nullptr;
    ANGLE_TRY(mBuffer.getMemory().map(device, offset, size, 0, &mapPointer));
    ASSERT(mapPointer);

    memcpy(mapPointer, data, size);

    mBuffer.getMemory().unmap(device);

    return vk::NoError();
}

const vk::Buffer &BufferVk::getVkBuffer() const
{
    return mBuffer;
}

}  // namespace rx

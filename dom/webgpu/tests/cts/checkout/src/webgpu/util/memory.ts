/**
 * Helper to exhaust VRAM until there is less than 64 MB of capacity. Returns
 * an opaque closure which can be called to free the allocated resources later.
 */
export async function exhaustVramUntilUnder64MB(device: GPUDevice) {
  const allocateUntilOom = async (device: GPUDevice, size: number) => {
    const buffers = [];
    for (;;) {
      device.pushErrorScope('out-of-memory');
      const buffer = device.createBuffer({ size, usage: GPUBufferUsage.STORAGE });
      if (await device.popErrorScope()) {
        return buffers;
      }
      buffers.push(buffer);
    }
  };

  const kLargeChunkSize = 512 * 1024 * 1024;
  const kSmallChunkSize = 64 * 1024 * 1024;
  const buffers = await allocateUntilOom(device, kLargeChunkSize);
  buffers.push(...(await allocateUntilOom(device, kSmallChunkSize)));
  return () => {
    buffers.forEach(buffer => buffer.destroy());
  };
}

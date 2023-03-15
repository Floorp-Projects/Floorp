export const description = `
Stress tests covering robustness when available VRAM is exhausted.
`;

import { makeTestGroup } from '../../common/framework/test_group.js';
import { GPUTest } from '../../webgpu/gpu_test.js';
import { exhaustVramUntilUnder64MB } from '../../webgpu/util/memory.js';

export const g = makeTestGroup(GPUTest);

g.test('vram_oom')
  .desc(`Tests that we can allocate buffers until we run out of VRAM.`)
  .fn(async t => {
    await exhaustVramUntilUnder64MB(t.device);
  });

g.test('get_mapped_range')
  .desc(
    `Tests getMappedRange on a mappedAtCreation GPUBuffer that failed allocation due
to OOM. This should throw a RangeError, but below a certain threshold may just
crash the page.`
  )
  .unimplemented();

g.test('map_after_vram_oom')
  .desc(
    `Allocates tons of buffers and textures with varying mapping states (unmappable,
mappable, mapAtCreation, mapAtCreation-then-unmapped) until OOM; then attempts
to mapAsync all the mappable objects.`
  )
  .unimplemented();

g.test('validation_vs_oom')
  .desc(
    `Tests that calls affected by both OOM and validation errors expose the
validation error with precedence.`
  )
  .unimplemented();

g.test('recovery')
  .desc(
    `Tests that after going VRAM-OOM, destroying allocated resources eventually
allows new resources to be allocated.`
  )
  .unimplemented();

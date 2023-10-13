import { kMaximumLimitBaseParams, makeLimitTestGroup } from './limit_utils.js';

const limit = 'maxComputeWorkgroupSizeY';
export const { g, description } = makeLimitTestGroup(limit);

g.test('createComputePipeline,at_over')
  .desc(`Test using createComputePipeline(Async) at and over ${limit} limit`)
  .params(kMaximumLimitBaseParams.combine('async', [false, true] as const))
  .fn(async t => {
    const { limitTest, testValueName, async } = t.params;
    await t.testMaxComputeWorkgroupSize(limitTest, testValueName, async, 'Y');
  });

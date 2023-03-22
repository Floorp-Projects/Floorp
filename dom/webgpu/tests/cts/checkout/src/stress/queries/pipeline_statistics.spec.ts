export const description = `
Stress tests for pipeline statistics queries.

TODO: pipeline statistics queries are removed from core; consider moving tests to another suite.
`;

import { makeTestGroup } from '../../common/framework/test_group.js';
import { GPUTest } from '../../webgpu/gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('render_pass_one_query_set')
  .desc(
    `Tests a huge number of pipeline statistics queries over a single query set in a
single render pass.`
  )
  .unimplemented();

g.test('render_pass_many_query_sets')
  .desc(
    `Tests a huge number of pipeline statistics queries over a huge number of query
sets in a single render pass.`
  )
  .unimplemented();

g.test('compute_pass_one_query_set')
  .desc(
    `Tests a huge number of pipeline statistics queries over a single query set in a
single compute pass.`
  )
  .unimplemented();

g.test('compute_pass_many_query_sets')
  .desc(
    `Tests a huge number of pipeline statistics queries over a huge number of query
sets in a single compute pass.`
  )
  .unimplemented();

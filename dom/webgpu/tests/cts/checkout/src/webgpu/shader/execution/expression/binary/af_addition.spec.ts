export const description = `
Execution Tests for non-matrix AbstractFloat addition expression
`;

import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';
import { TypeAbstractFloat, TypeVec } from '../../../../util/conversion.js';
import { FP, FPVector } from '../../../../util/floating_point.js';
import { sparseF64Range, sparseVectorF64Range } from '../../../../util/math.js';
import { makeCaseCache } from '../case_cache.js';
import { onlyConstInputSource, run } from '../expression.js';

import { abstractBinary } from './binary.js';

const additionVectorScalarInterval = (v: number[], s: number): FPVector => {
  return FP.abstract.toVector(v.map(e => FP.abstract.additionInterval(e, s)));
};

const additionScalarVectorInterval = (s: number, v: number[]): FPVector => {
  return FP.abstract.toVector(v.map(e => FP.abstract.additionInterval(s, e)));
};

export const g = makeTestGroup(GPUTest);

export const d = makeCaseCache('binary/af_addition', {
  scalar: () => {
    return FP.abstract.generateScalarPairToIntervalCases(
      sparseF64Range(),
      sparseF64Range(),
      'finite',
      FP.abstract.additionInterval
    );
  },
  vec2_scalar: () => {
    return FP.abstract.generateVectorScalarToVectorCases(
      sparseVectorF64Range(2),
      sparseF64Range(),
      'finite',
      additionVectorScalarInterval
    );
  },
  vec3_scalar: () => {
    return FP.abstract.generateVectorScalarToVectorCases(
      sparseVectorF64Range(3),
      sparseF64Range(),
      'finite',
      additionVectorScalarInterval
    );
  },
  vec4_scalar: () => {
    return FP.abstract.generateVectorScalarToVectorCases(
      sparseVectorF64Range(4),
      sparseF64Range(),
      'finite',
      additionVectorScalarInterval
    );
  },
  scalar_vec2: () => {
    return FP.abstract.generateScalarVectorToVectorCases(
      sparseF64Range(),
      sparseVectorF64Range(2),
      'finite',
      additionScalarVectorInterval
    );
  },
  scalar_vec3: () => {
    return FP.abstract.generateScalarVectorToVectorCases(
      sparseF64Range(),
      sparseVectorF64Range(3),
      'finite',
      additionScalarVectorInterval
    );
  },
  scalar_vec4: () => {
    return FP.abstract.generateScalarVectorToVectorCases(
      sparseF64Range(),
      sparseVectorF64Range(4),
      'finite',
      additionScalarVectorInterval
    );
  },
});

g.test('scalar')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x + y, where x and y are scalars
Accuracy: Correctly rounded
`
  )
  .params(u => u.combine('inputSource', onlyConstInputSource))
  .fn(async t => {
    const cases = await d.get('scalar');
    await run(
      t,
      abstractBinary('+'),
      [TypeAbstractFloat, TypeAbstractFloat],
      TypeAbstractFloat,
      t.params,
      cases
    );
  });

g.test('vector')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x + y, where x and y are vectors
Accuracy: Correctly rounded
`
  )
  .params(u =>
    u.combine('inputSource', onlyConstInputSource).combine('vectorize', [2, 3, 4] as const)
  )
  .fn(async t => {
    const cases = await d.get('scalar'); // Using vectorize to generate vector cases based on scalar cases
    await run(
      t,
      abstractBinary('+'),
      [TypeAbstractFloat, TypeAbstractFloat],
      TypeAbstractFloat,
      t.params,
      cases
    );
  });

g.test('vector_scalar')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x + y, where x is a vector and y is a scalar
Accuracy: Correctly rounded
`
  )
  .params(u => u.combine('inputSource', onlyConstInputSource).combine('dim', [2, 3, 4] as const))
  .fn(async t => {
    const dim = t.params.dim;
    const cases = await d.get(`vec${dim}_scalar`);
    await run(
      t,
      abstractBinary('+'),
      [TypeVec(dim, TypeAbstractFloat), TypeAbstractFloat],
      TypeVec(dim, TypeAbstractFloat),
      t.params,
      cases
    );
  });

g.test('scalar_vector')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x + y, where x is a scalar and y is a vector
Accuracy: Correctly rounded
`
  )
  .params(u => u.combine('inputSource', onlyConstInputSource).combine('dim', [2, 3, 4] as const))
  .fn(async t => {
    const dim = t.params.dim;
    const cases = await d.get(`scalar_vec${dim}`);
    await run(
      t,
      abstractBinary('+'),
      [TypeAbstractFloat, TypeVec(dim, TypeAbstractFloat)],
      TypeVec(dim, TypeAbstractFloat),
      t.params,
      cases
    );
  });

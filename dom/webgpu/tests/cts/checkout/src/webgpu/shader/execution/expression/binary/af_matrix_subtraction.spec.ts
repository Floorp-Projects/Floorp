export const description = `
Execution Tests for matrix AbstractFloat subtraction expression
`;

import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';
import { TypeAbstractFloat, TypeMat } from '../../../../util/conversion.js';
import { FP } from '../../../../util/floating_point.js';
import { sparseMatrixF64Range } from '../../../../util/math.js';
import { makeCaseCache } from '../case_cache.js';
import { onlyConstInputSource, run } from '../expression.js';

import { abstractBinary } from './binary.js';

export const g = makeTestGroup(GPUTest);

export const d = makeCaseCache('abstractBinary/af_matrix_subtraction', {
  mat2x2: () => {
    return FP.abstract.generateMatrixPairToMatrixCases(
      sparseMatrixF64Range(2, 2),
      sparseMatrixF64Range(2, 2),
      'finite',
      FP.abstract.subtractionMatrixMatrixInterval
    );
  },
  mat2x3: () => {
    return FP.abstract.generateMatrixPairToMatrixCases(
      sparseMatrixF64Range(2, 3),
      sparseMatrixF64Range(2, 3),
      'finite',
      FP.abstract.subtractionMatrixMatrixInterval
    );
  },
  mat2x4: () => {
    return FP.abstract.generateMatrixPairToMatrixCases(
      sparseMatrixF64Range(2, 4),
      sparseMatrixF64Range(2, 4),
      'finite',
      FP.abstract.subtractionMatrixMatrixInterval
    );
  },
  mat3x2: () => {
    return FP.abstract.generateMatrixPairToMatrixCases(
      sparseMatrixF64Range(3, 2),
      sparseMatrixF64Range(3, 2),
      'finite',
      FP.abstract.subtractionMatrixMatrixInterval
    );
  },
  mat3x3: () => {
    return FP.abstract.generateMatrixPairToMatrixCases(
      sparseMatrixF64Range(3, 3),
      sparseMatrixF64Range(3, 3),
      'finite',
      FP.abstract.subtractionMatrixMatrixInterval
    );
  },
  mat3x4: () => {
    return FP.abstract.generateMatrixPairToMatrixCases(
      sparseMatrixF64Range(3, 4),
      sparseMatrixF64Range(3, 4),
      'finite',
      FP.abstract.subtractionMatrixMatrixInterval
    );
  },
  mat4x2: () => {
    return FP.abstract.generateMatrixPairToMatrixCases(
      sparseMatrixF64Range(4, 2),
      sparseMatrixF64Range(4, 2),
      'finite',
      FP.abstract.subtractionMatrixMatrixInterval
    );
  },
  mat4x3: () => {
    return FP.abstract.generateMatrixPairToMatrixCases(
      sparseMatrixF64Range(4, 3),
      sparseMatrixF64Range(4, 3),
      'finite',
      FP.abstract.subtractionMatrixMatrixInterval
    );
  },
  mat4x4: () => {
    return FP.abstract.generateMatrixPairToMatrixCases(
      sparseMatrixF64Range(4, 4),
      sparseMatrixF64Range(4, 4),
      'finite',
      FP.abstract.subtractionMatrixMatrixInterval
    );
  },
});

g.test('matrix')
  .specURL('https://www.w3.org/TR/WGSL/#floating-point-evaluation')
  .desc(
    `
Expression: x - y, where x and y are matrices
Accuracy: Correctly rounded
`
  )
  .params(u =>
    u
      .combine('inputSource', onlyConstInputSource)
      .combine('cols', [2, 3, 4] as const)
      .combine('rows', [2, 3, 4] as const)
  )
  .fn(async t => {
    const cols = t.params.cols;
    const rows = t.params.rows;
    const cases = await d.get(`mat${cols}x${rows}`);
    await run(
      t,
      abstractBinary('-'),
      [TypeMat(cols, rows, TypeAbstractFloat), TypeMat(cols, rows, TypeAbstractFloat)],
      TypeMat(cols, rows, TypeAbstractFloat),
      t.params,
      cases
    );
  });

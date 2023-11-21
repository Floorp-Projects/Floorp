import { kUnitCaseParamsBuilder } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { getGPU } from '../../../../../common/util/navigator_gpu.js';
import { assert, range, reorder, ReorderOrder } from '../../../../../common/util/util.js';
import { getDefaultLimitsForAdapter } from '../../../../capability_info.js';
import { GPUTestBase } from '../../../../gpu_test.js';

type GPUSupportedLimit = keyof GPUSupportedLimits;

export const kCreatePipelineTypes = [
  'createRenderPipeline',
  'createRenderPipelineWithFragmentStage',
  'createComputePipeline',
] as const;
export type CreatePipelineType = (typeof kCreatePipelineTypes)[number];

export const kRenderEncoderTypes = ['render', 'renderBundle'] as const;
export type RenderEncoderType = (typeof kRenderEncoderTypes)[number];

export const kEncoderTypes = ['compute', 'render', 'renderBundle'] as const;
export type EncoderType = (typeof kEncoderTypes)[number];

export const kBindGroupTests = ['sameGroup', 'differentGroups'] as const;
export type BindGroupTest = (typeof kBindGroupTests)[number];

export const kBindingCombinations = [
  'vertex',
  'fragment',
  'vertexAndFragmentWithPossibleVertexStageOverflow',
  'vertexAndFragmentWithPossibleFragmentStageOverflow',
  'compute',
] as const;
export type BindingCombination = (typeof kBindingCombinations)[number];

export function getPipelineTypeForBindingCombination(bindingCombination: BindingCombination) {
  switch (bindingCombination) {
    case 'vertex':
      return 'createRenderPipeline';
    case 'fragment':
    case 'vertexAndFragmentWithPossibleVertexStageOverflow':
    case 'vertexAndFragmentWithPossibleFragmentStageOverflow':
      return 'createRenderPipelineWithFragmentStage';
    case 'compute':
      return 'createComputePipeline';
  }
}

function getBindGroupIndex(bindGroupTest: BindGroupTest, i: number) {
  switch (bindGroupTest) {
    case 'sameGroup':
      return 0;
    case 'differentGroups':
      return i % 3;
  }
}

function getWGSLBindings(
  order: ReorderOrder,
  bindGroupTest: BindGroupTest,
  storageDefinitionWGSLSnippetFn: (i: number, j: number) => string,
  numBindings: number,
  id: number
) {
  return reorder(
    order,
    range(
      numBindings,
      i =>
        `@group(${getBindGroupIndex(
          bindGroupTest,
          i
        )}) @binding(${i}) ${storageDefinitionWGSLSnippetFn(i, id)};`
    )
  ).join('\n        ');
}

export function getPerStageWGSLForBindingCombinationImpl(
  bindingCombination: BindingCombination,
  order: ReorderOrder,
  bindGroupTest: BindGroupTest,
  storageDefinitionWGSLSnippetFn: (i: number, j: number) => string,
  bodyFn: (numBindings: number, set: number) => string,
  numBindings: number,
  extraWGSL = ''
) {
  switch (bindingCombination) {
    case 'vertex':
      return `
        ${extraWGSL}

        ${getWGSLBindings(order, bindGroupTest, storageDefinitionWGSLSnippetFn, numBindings, 0)}

        @vertex fn mainVS() -> @builtin(position) vec4f {
          ${bodyFn(numBindings, 0)}
          return vec4f(0);
        }
      `;
    case 'fragment':
      return `
        ${extraWGSL}

        ${getWGSLBindings(order, bindGroupTest, storageDefinitionWGSLSnippetFn, numBindings, 0)}

        @vertex fn mainVS() -> @builtin(position) vec4f {
          return vec4f(0);
        }

        @fragment fn mainFS() {
          ${bodyFn(numBindings, 0)}
        }
      `;
    case 'vertexAndFragmentWithPossibleVertexStageOverflow': {
      return `
        ${extraWGSL}

        ${getWGSLBindings(order, bindGroupTest, storageDefinitionWGSLSnippetFn, numBindings, 0)}

        ${getWGSLBindings(order, bindGroupTest, storageDefinitionWGSLSnippetFn, numBindings - 1, 1)}

        @vertex fn mainVS() -> @builtin(position) vec4f {
          ${bodyFn(numBindings, 0)}
          return vec4f(0);
        }

        @fragment fn mainFS() {
          ${bodyFn(numBindings - 1, 1)}
        }
      `;
    }
    case 'vertexAndFragmentWithPossibleFragmentStageOverflow': {
      return `
        ${extraWGSL}

        ${getWGSLBindings(order, bindGroupTest, storageDefinitionWGSLSnippetFn, numBindings - 1, 0)}

        ${getWGSLBindings(order, bindGroupTest, storageDefinitionWGSLSnippetFn, numBindings, 1)}

        @vertex fn mainVS() -> @builtin(position) vec4f {
          ${bodyFn(numBindings - 1, 0)}
          return vec4f(0);
        }

        @fragment fn mainFS() {
          ${bodyFn(numBindings, 1)}
        }
      `;
    }
    case 'compute':
      return `
        ${extraWGSL}
        ${getWGSLBindings(order, bindGroupTest, storageDefinitionWGSLSnippetFn, numBindings, 0)}
        @group(3) @binding(0) var<storage, read_write> d: f32;
        @compute @workgroup_size(1) fn main() {
          ${bodyFn(numBindings, 0)}
        }
      `;
      break;
  }
}

export function getPerStageWGSLForBindingCombination(
  bindingCombination: BindingCombination,
  order: ReorderOrder,
  bindGroupTest: BindGroupTest,
  storageDefinitionWGSLSnippetFn: (i: number, j: number) => string,
  usageWGSLSnippetFn: (i: number, j: number) => string,
  numBindings: number,
  extraWGSL = ''
) {
  return getPerStageWGSLForBindingCombinationImpl(
    bindingCombination,
    order,
    bindGroupTest,
    storageDefinitionWGSLSnippetFn,
    (numBindings: number, set: number) =>
      `${range(numBindings, i => usageWGSLSnippetFn(i, set)).join('\n          ')}`,
    numBindings,
    extraWGSL
  );
}

export function getPerStageWGSLForBindingCombinationStorageTextures(
  bindingCombination: BindingCombination,
  order: ReorderOrder,
  bindGroupTest: BindGroupTest,
  storageDefinitionWGSLSnippetFn: (i: number, j: number) => string,
  usageWGSLSnippetFn: (i: number, j: number) => string,
  numBindings: number,
  extraWGSL = ''
) {
  return getPerStageWGSLForBindingCombinationImpl(
    bindingCombination,
    order,
    bindGroupTest,
    storageDefinitionWGSLSnippetFn,
    (numBindings: number, set: number) =>
      `${range(numBindings, i => usageWGSLSnippetFn(i, set)).join('\n          ')}`,
    numBindings,
    extraWGSL
  );
}

export const kLimitModes = ['defaultLimit', 'adapterLimit'] as const;
export type LimitMode = (typeof kLimitModes)[number];
export type LimitsRequest = Record<string, LimitMode>;

export const kMaximumTestValues = ['atLimit', 'overLimit'] as const;
export type MaximumTestValue = (typeof kMaximumTestValues)[number];

export function getMaximumTestValue(limit: number, testValue: MaximumTestValue) {
  switch (testValue) {
    case 'atLimit':
      return limit;
    case 'overLimit':
      return limit + 1;
  }
}

export const kMinimumTestValues = ['atLimit', 'underLimit'] as const;
export type MinimumTestValue = (typeof kMinimumTestValues)[number];

export const kMaximumLimitValueTests = [
  'atDefault',
  'underDefault',
  'betweenDefaultAndMaximum',
  'atMaximum',
  'overMaximum',
] as const;
export type MaximumLimitValueTest = (typeof kMaximumLimitValueTests)[number];

export function getLimitValue(
  defaultLimit: number,
  maximumLimit: number,
  limitValueTest: MaximumLimitValueTest
) {
  switch (limitValueTest) {
    case 'atDefault':
      return defaultLimit;
    case 'underDefault':
      return defaultLimit - 1;
    case 'betweenDefaultAndMaximum':
      // The result can be larger than maximum i32.
      return Math.floor((defaultLimit + maximumLimit) / 2);
    case 'atMaximum':
      return maximumLimit;
    case 'overMaximum':
      return maximumLimit + 1;
  }
}

export const kMinimumLimitValueTests = [
  'atDefault',
  'overDefault',
  'betweenDefaultAndMinimum',
  'atMinimum',
  'underMinimum',
] as const;
export type MinimumLimitValueTest = (typeof kMinimumLimitValueTests)[number];

export function getDefaultLimitForAdapter(adapter: GPUAdapter, limit: GPUSupportedLimit): number {
  const limitInfo = getDefaultLimitsForAdapter(adapter);
  return limitInfo[limit as keyof typeof limitInfo].default;
}

export type DeviceAndLimits = {
  device: GPUDevice;
  defaultLimit: number;
  adapterLimit: number;
  requestedLimit: number;
  actualLimit: number;
};

export type SpecificLimitTestInputs = DeviceAndLimits & {
  testValue: number;
  shouldError: boolean;
};

export type MaximumLimitTestInputs = SpecificLimitTestInputs & {
  testValueName: MaximumTestValue;
};

const kMinimumLimits = new Set<GPUSupportedLimit>([
  'minUniformBufferOffsetAlignment',
  'minStorageBufferOffsetAlignment',
]);

/**
 * Adds the default parameters to a limit test
 */
export const kMaximumLimitBaseParams = kUnitCaseParamsBuilder
  .combine('limitTest', kMaximumLimitValueTests)
  .combine('testValueName', kMaximumTestValues);

export const kMinimumLimitBaseParams = kUnitCaseParamsBuilder
  .combine('limitTest', kMinimumLimitValueTests)
  .combine('testValueName', kMinimumTestValues);

export class LimitTestsImpl extends GPUTestBase {
  _adapter: GPUAdapter | null = null;
  _device: GPUDevice | undefined = undefined;
  limit: GPUSupportedLimit = '' as GPUSupportedLimit;
  defaultLimit = 0;
  adapterLimit = 0;

  override async init() {
    await super.init();
    const gpu = getGPU(this.rec);
    this._adapter = await gpu.requestAdapter();
    const limit = this.limit;
    this.defaultLimit = getDefaultLimitForAdapter(this.adapter, limit);
    this.adapterLimit = this.adapter.limits[limit] as number;
    assert(!Number.isNaN(this.defaultLimit));
    assert(!Number.isNaN(this.adapterLimit));
  }

  get adapter(): GPUAdapter {
    assert(this._adapter !== undefined);
    return this._adapter!;
  }

  override get device(): GPUDevice {
    assert(this._device !== undefined, 'device is only valid in _testThenDestroyDevice callback');
    return this._device;
  }

  async requestDeviceWithLimits(
    adapter: GPUAdapter,
    requiredLimits: Record<string, number>,
    shouldReject: boolean,
    requiredFeatures?: GPUFeatureName[]
  ) {
    if (shouldReject) {
      this.shouldReject('OperationError', adapter.requestDevice({ requiredLimits }), {
        allowMissingStack: true,
      });
      return undefined;
    } else {
      return await adapter.requestDevice({ requiredLimits, requiredFeatures });
    }
  }

  getDefaultOrAdapterLimit(limit: GPUSupportedLimit, limitMode: LimitMode) {
    switch (limitMode) {
      case 'defaultLimit':
        return getDefaultLimitForAdapter(this.adapter, limit);
      case 'adapterLimit':
        return this.adapter.limits[limit];
    }
  }

  /**
   * Gets a device with the adapter a requested limit and checks that that limit
   * is correct or that the device failed to create if the requested limit is
   * beyond the maximum supported by the device.
   */
  async _getDeviceWithSpecificLimit(
    requestedLimit: number,
    extraLimits?: LimitsRequest,
    features?: GPUFeatureName[]
  ): Promise<DeviceAndLimits | undefined> {
    const { adapter, limit, adapterLimit, defaultLimit } = this;

    const requiredLimits: Record<string, number> = {};
    requiredLimits[limit] = requestedLimit;

    if (extraLimits) {
      for (const [extraLimitStr, limitMode] of Object.entries(extraLimits)) {
        const extraLimit = extraLimitStr as GPUSupportedLimit;
        requiredLimits[extraLimit] =
          limitMode === 'defaultLimit'
            ? getDefaultLimitForAdapter(adapter, extraLimit)
            : (adapter.limits[extraLimit] as number);
      }
    }

    const shouldReject = kMinimumLimits.has(limit)
      ? requestedLimit < adapterLimit
      : requestedLimit > adapterLimit;

    const device = await this.requestDeviceWithLimits(
      adapter,
      requiredLimits,
      shouldReject,
      features
    );
    const actualLimit = (device ? device.limits[limit] : 0) as number;

    if (shouldReject) {
      this.expect(!device, 'expected no device');
    } else {
      if (kMinimumLimits.has(limit)) {
        if (requestedLimit <= defaultLimit) {
          this.expect(
            actualLimit === requestedLimit,
            `expected actual actualLimit: ${actualLimit} to equal defaultLimit: ${requestedLimit}`
          );
        } else {
          this.expect(
            actualLimit === defaultLimit,
            `expected actual actualLimit: ${actualLimit} to equal defaultLimit: ${defaultLimit}`
          );
        }
      } else {
        if (requestedLimit <= defaultLimit) {
          this.expect(
            actualLimit === defaultLimit,
            `expected actual actualLimit: ${actualLimit} to equal defaultLimit: ${defaultLimit}`
          );
        } else {
          this.expect(
            actualLimit === requestedLimit,
            `expected actual actualLimit: ${actualLimit} to equal requestedLimit: ${requestedLimit}`
          );
        }
      }
    }

    return device ? { device, defaultLimit, adapterLimit, requestedLimit, actualLimit } : undefined;
  }

  /**
   * Gets a device with the adapter a requested limit and checks that that limit
   * is correct or that the device failed to create if the requested limit is
   * beyond the maximum supported by the device.
   */
  async _getDeviceWithRequestedMaximumLimit(
    limitValueTest: MaximumLimitValueTest,
    extraLimits?: LimitsRequest,
    features?: GPUFeatureName[]
  ): Promise<DeviceAndLimits | undefined> {
    const { defaultLimit, adapterLimit: maximumLimit } = this;

    const requestedLimit = getLimitValue(defaultLimit, maximumLimit, limitValueTest);
    return this._getDeviceWithSpecificLimit(requestedLimit, extraLimits, features);
  }

  /**
   * Call the given function and check no WebGPU errors are leaked.
   */
  async _testThenDestroyDevice(
    deviceAndLimits: DeviceAndLimits,
    testValue: number,
    fn: (inputs: SpecificLimitTestInputs) => void | Promise<void>
  ) {
    assert(!this._device);

    const { device, actualLimit } = deviceAndLimits;
    this._device = device;

    const shouldError = kMinimumLimits.has(this.limit)
      ? testValue < actualLimit
      : testValue > actualLimit;

    device.pushErrorScope('internal');
    device.pushErrorScope('out-of-memory');
    device.pushErrorScope('validation');

    await fn({ ...deviceAndLimits, testValue, shouldError });

    const validationError = await device.popErrorScope();
    const outOfMemoryError = await device.popErrorScope();
    const internalError = await device.popErrorScope();

    this.expect(!validationError, `unexpected validation error: ${validationError?.message || ''}`);
    this.expect(
      !outOfMemoryError,
      `unexpected out-of-memory error: ${outOfMemoryError?.message || ''}`
    );
    this.expect(!internalError, `unexpected internal error: ${internalError?.message || ''}`);

    device.destroy();
    this._device = undefined;
  }

  /**
   * Creates a device with a specific limit.
   * If the limit of over the maximum we expect an exception
   * If the device is created then we call a test function, checking
   * that the function does not leak any GPU errors.
   */
  async testDeviceWithSpecificLimits(
    deviceLimitValue: number,
    testValue: number,
    fn: (inputs: SpecificLimitTestInputs) => void | Promise<void>,
    extraLimits?: LimitsRequest,
    features?: GPUFeatureName[]
  ) {
    assert(!this._device);

    const deviceAndLimits = await this._getDeviceWithSpecificLimit(
      deviceLimitValue,
      extraLimits,
      features
    );
    // If we request over the limit requestDevice will throw
    if (!deviceAndLimits) {
      return;
    }

    await this._testThenDestroyDevice(deviceAndLimits, testValue, fn);
  }

  /**
   * Creates a device with the limit defined by LimitValueTest.
   * If the limit of over the maximum we expect an exception
   * If the device is created then we call a test function, checking
   * that the function does not leak any GPU errors.
   */
  async testDeviceWithRequestedMaximumLimits(
    limitTest: MaximumLimitValueTest,
    testValueName: MaximumTestValue,
    fn: (inputs: MaximumLimitTestInputs) => void | Promise<void>,
    extraLimits?: LimitsRequest
  ) {
    assert(!this._device);

    const deviceAndLimits = await this._getDeviceWithRequestedMaximumLimit(limitTest, extraLimits);
    // If we request over the limit requestDevice will throw
    if (!deviceAndLimits) {
      return;
    }

    const { actualLimit } = deviceAndLimits;
    const testValue = getMaximumTestValue(actualLimit, testValueName);

    await this._testThenDestroyDevice(
      deviceAndLimits,
      testValue,
      async (inputs: SpecificLimitTestInputs) => {
        await fn({ ...inputs, testValueName });
      }
    );
  }

  /**
   * Calls a function that expects a GPU error if shouldError is true
   */
  // MAINTENANCE_TODO: Remove this duplicated code with GPUTest if possible
  async expectGPUErrorAsync<R>(
    filter: GPUErrorFilter,
    fn: () => R,
    shouldError: boolean = true,
    msg = ''
  ): Promise<R> {
    const { device } = this;

    device.pushErrorScope(filter);
    const returnValue = fn();
    if (returnValue instanceof Promise) {
      await returnValue;
    }

    const error = await device.popErrorScope();
    this.expect(
      !!error === shouldError,
      `${error?.message || 'no error when one was expected'}: ${msg}`
    );

    return returnValue;
  }

  /** Expect that the provided promise rejects, with the provided exception name. */
  async shouldRejectConditionally(
    expectedName: string,
    p: Promise<unknown>,
    shouldReject: boolean,
    message?: string
  ): Promise<void> {
    if (shouldReject) {
      this.shouldReject(expectedName, p, { message });
    } else {
      this.shouldResolve(p, message);
    }

    // We need to explicitly wait for the promise because the device may be
    // destroyed immediately after returning from this function.
    try {
      await p;
    } catch (e) {
      //
    }
  }

  /**
   * Calls a function that expects a validation error if shouldError is true
   */
  override async expectValidationError<R>(
    fn: () => R,
    shouldError: boolean = true,
    msg = ''
  ): Promise<R> {
    return this.expectGPUErrorAsync('validation', fn, shouldError, msg);
  }

  /**
   * Calls a function that expects to not generate a validation error
   */
  async expectNoValidationError<R>(fn: () => R, msg = ''): Promise<R> {
    return this.expectGPUErrorAsync('validation', fn, false, msg);
  }

  /**
   * Calls a function that might expect a validation error.
   * if shouldError is true then expect a validation error,
   * if shouldError is false then ignore out-of-memory errors.
   */
  async testForValidationErrorWithPossibleOutOfMemoryError<R>(
    fn: () => R,
    shouldError: boolean = true,
    msg = ''
  ): Promise<R> {
    const { device } = this;

    if (!shouldError) {
      device.pushErrorScope('out-of-memory');
      const result = fn();
      await device.popErrorScope();
      return result;
    }

    // Validation should fail before out-of-memory so there is no need to check
    // for out-of-memory here.
    device.pushErrorScope('validation');
    const returnValue = fn();
    const validationError = await device.popErrorScope();

    this.expect(
      !!validationError,
      `${validationError?.message || 'no error when one was expected'}: ${msg}`
    );

    return returnValue;
  }

  getGroupIndexWGSLForPipelineType(pipelineType: CreatePipelineType, groupIndex: number) {
    switch (pipelineType) {
      case 'createRenderPipeline':
        return `
          @group(${groupIndex}) @binding(0) var<uniform> v: f32;
          @vertex fn mainVS() -> @builtin(position) vec4f {
            return vec4f(v);
          }
        `;
      case 'createRenderPipelineWithFragmentStage':
        return `
          @group(${groupIndex}) @binding(0) var<uniform> v: f32;
          @vertex fn mainVS() -> @builtin(position) vec4f {
            return vec4f(v);
          }
          @fragment fn mainFS() -> @location(0) vec4f {
            return vec4f(1);
          }
        `;
      case 'createComputePipeline':
        return `
          @group(${groupIndex}) @binding(0) var<uniform> v: f32;
          @compute @workgroup_size(1) fn main() {
            _ = v;
          }
        `;
        break;
    }
  }

  getBindingIndexWGSLForPipelineType(pipelineType: CreatePipelineType, bindingIndex: number) {
    switch (pipelineType) {
      case 'createRenderPipeline':
        return `
          @group(0) @binding(${bindingIndex}) var<uniform> v: f32;
          @vertex fn mainVS() -> @builtin(position) vec4f {
            return vec4f(v);
          }
        `;
      case 'createRenderPipelineWithFragmentStage':
        return `
          @group(0) @binding(${bindingIndex}) var<uniform> v: f32;
          @vertex fn mainVS() -> @builtin(position) vec4f {
            return vec4f(v);
          }
          @fragment fn mainFS() -> @location(0) vec4f {
            return vec4f(1);
          }
        `;
      case 'createComputePipeline':
        return `
          @group(0) @binding(${bindingIndex}) var<uniform> v: f32;
          @compute @workgroup_size(1) fn main() {
            _ = v;
          }
        `;
        break;
    }
  }

  _createRenderPipelineDescriptor(module: GPUShaderModule): GPURenderPipelineDescriptor {
    return {
      layout: 'auto',
      vertex: {
        module,
        entryPoint: 'mainVS',
      },
    };
  }

  _createRenderPipelineDescriptorWithFragmentShader(
    module: GPUShaderModule
  ): GPURenderPipelineDescriptor {
    return {
      layout: 'auto',
      vertex: {
        module,
        entryPoint: 'mainVS',
      },
      fragment: {
        module,
        entryPoint: 'mainFS',
        targets: [],
      },
      depthStencil: {
        format: 'depth24plus-stencil8',
        depthWriteEnabled: true,
        depthCompare: 'always',
      },
    };
  }

  _createComputePipelineDescriptor(module: GPUShaderModule): GPUComputePipelineDescriptor {
    return {
      layout: 'auto',
      compute: {
        module,
        entryPoint: 'main',
      },
    };
  }

  createPipeline(createPipelineType: CreatePipelineType, module: GPUShaderModule) {
    const { device } = this;

    switch (createPipelineType) {
      case 'createRenderPipeline':
        return device.createRenderPipeline(this._createRenderPipelineDescriptor(module));
        break;
      case 'createRenderPipelineWithFragmentStage':
        return device.createRenderPipeline(
          this._createRenderPipelineDescriptorWithFragmentShader(module)
        );
        break;
      case 'createComputePipeline':
        return device.createComputePipeline(this._createComputePipelineDescriptor(module));
        break;
    }
  }

  createPipelineAsync(createPipelineType: CreatePipelineType, module: GPUShaderModule) {
    const { device } = this;

    switch (createPipelineType) {
      case 'createRenderPipeline':
        return device.createRenderPipelineAsync(this._createRenderPipelineDescriptor(module));
      case 'createRenderPipelineWithFragmentStage':
        return device.createRenderPipelineAsync(
          this._createRenderPipelineDescriptorWithFragmentShader(module)
        );
      case 'createComputePipeline':
        return device.createComputePipelineAsync(this._createComputePipelineDescriptor(module));
    }
  }

  async testCreatePipeline(
    createPipelineType: CreatePipelineType,
    async: boolean,
    module: GPUShaderModule,
    shouldError: boolean,
    msg = ''
  ) {
    if (async) {
      await this.shouldRejectConditionally(
        'GPUPipelineError',
        this.createPipelineAsync(createPipelineType, module),
        shouldError,
        msg
      );
    } else {
      await this.expectValidationError(
        () => {
          this.createPipeline(createPipelineType, module);
        },
        shouldError,
        msg
      );
    }
  }

  async testCreateRenderPipeline(
    pipelineDescriptor: GPURenderPipelineDescriptor,
    async: boolean,
    shouldError: boolean,
    msg = ''
  ) {
    const { device } = this;
    if (async) {
      await this.shouldRejectConditionally(
        'GPUPipelineError',
        device.createRenderPipelineAsync(pipelineDescriptor),
        shouldError,
        msg
      );
    } else {
      await this.expectValidationError(
        () => {
          device.createRenderPipeline(pipelineDescriptor);
        },
        shouldError,
        msg
      );
    }
  }

  async testMaxComputeWorkgroupSize(
    limitTest: MaximumLimitValueTest,
    testValueName: MaximumTestValue,
    async: boolean,
    axis: 'X' | 'Y' | 'Z'
  ) {
    const kExtraLimits: LimitsRequest = {
      maxComputeInvocationsPerWorkgroup: 'adapterLimit',
    };

    await this.testDeviceWithRequestedMaximumLimits(
      limitTest,
      testValueName,
      async ({ device, testValue, actualLimit, shouldError }) => {
        if (testValue > device.limits.maxComputeInvocationsPerWorkgroup) {
          return;
        }

        const size = [1, 1, 1];
        size[axis.codePointAt(0)! - 'X'.codePointAt(0)!] = testValue;
        const { module, code } = this.getModuleForWorkgroupSize(size);

        await this.testCreatePipeline(
          'createComputePipeline',
          async,
          module,
          shouldError,
          `size: ${testValue}, limit: ${actualLimit}\n${code}`
        );
      },
      kExtraLimits
    );
  }

  /**
   * Creates an GPURenderCommandsMixin setup with some initial state.
   */
  _getGPURenderCommandsMixin(encoderType: RenderEncoderType) {
    const { device } = this;

    switch (encoderType) {
      case 'render': {
        const buffer = this.trackForCleanup(
          device.createBuffer({
            size: 16,
            usage: GPUBufferUsage.UNIFORM,
          })
        );

        const texture = this.trackForCleanup(
          device.createTexture({
            size: [1, 1],
            format: 'rgba8unorm',
            usage: GPUTextureUsage.RENDER_ATTACHMENT,
          })
        );

        const layout = device.createBindGroupLayout({
          entries: [
            {
              binding: 0,
              visibility: GPUShaderStage.VERTEX,
              buffer: {},
            },
          ],
        });

        const bindGroup = device.createBindGroup({
          layout,
          entries: [
            {
              binding: 0,
              resource: { buffer },
            },
          ],
        });

        const encoder = device.createCommandEncoder();
        const mixin = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: texture.createView(),
              loadOp: 'clear',
              storeOp: 'store',
            },
          ],
        });

        return {
          mixin,
          bindGroup,
          prep() {
            mixin.end();
          },
          test() {
            encoder.finish();
          },
        };
        break;
      }

      case 'renderBundle': {
        const buffer = this.trackForCleanup(
          device.createBuffer({
            size: 16,
            usage: GPUBufferUsage.UNIFORM,
          })
        );

        const layout = device.createBindGroupLayout({
          entries: [
            {
              binding: 0,
              visibility: GPUShaderStage.VERTEX,
              buffer: {},
            },
          ],
        });

        const bindGroup = device.createBindGroup({
          layout,
          entries: [
            {
              binding: 0,
              resource: { buffer },
            },
          ],
        });

        const mixin = device.createRenderBundleEncoder({
          colorFormats: ['rgba8unorm'],
        });

        return {
          mixin,
          bindGroup,
          prep() {},
          test() {
            mixin.finish();
          },
        };
        break;
      }
    }
  }

  /**
   * Tests a method on GPURenderCommandsMixin
   * The function will be called with the mixin.
   */
  async testGPURenderCommandsMixin(
    encoderType: RenderEncoderType,
    fn: ({ mixin }: { mixin: GPURenderCommandsMixin }) => void,
    shouldError: boolean,
    msg = ''
  ) {
    const { mixin, prep, test } = this._getGPURenderCommandsMixin(encoderType);
    fn({ mixin });
    prep();

    await this.expectValidationError(test, shouldError, msg);
  }

  /**
   * Creates GPUBindingCommandsMixin setup with some initial state.
   */
  _getGPUBindingCommandsMixin(encoderType: EncoderType) {
    const { device } = this;

    switch (encoderType) {
      case 'compute': {
        const buffer = this.trackForCleanup(
          device.createBuffer({
            size: 16,
            usage: GPUBufferUsage.UNIFORM,
          })
        );

        const layout = device.createBindGroupLayout({
          entries: [
            {
              binding: 0,
              visibility: GPUShaderStage.COMPUTE,
              buffer: {},
            },
          ],
        });

        const bindGroup = device.createBindGroup({
          layout,
          entries: [
            {
              binding: 0,
              resource: { buffer },
            },
          ],
        });

        const encoder = device.createCommandEncoder();
        const mixin = encoder.beginComputePass();
        return {
          mixin,
          bindGroup,
          prep() {
            mixin.end();
          },
          test() {
            encoder.finish();
          },
        };
        break;
      }
      case 'render':
        return this._getGPURenderCommandsMixin('render');
      case 'renderBundle':
        return this._getGPURenderCommandsMixin('renderBundle');
    }
  }

  /**
   * Tests a method on GPUBindingCommandsMixin
   * The function pass will be called with the mixin and a bindGroup
   */
  async testGPUBindingCommandsMixin(
    encoderType: EncoderType,
    fn: ({ bindGroup }: { mixin: GPUBindingCommandsMixin; bindGroup: GPUBindGroup }) => void,
    shouldError: boolean,
    msg = ''
  ) {
    const { mixin, bindGroup, prep, test } = this._getGPUBindingCommandsMixin(encoderType);
    fn({ mixin, bindGroup });
    prep();

    await this.expectValidationError(test, shouldError, msg);
  }

  getModuleForWorkgroupSize(size: number[]) {
    const { device } = this;
    const code = `
      @group(0) @binding(0) var<storage, read_write> d: f32;
      @compute @workgroup_size(${size.join(',')}) fn main() {
        d = 0;
      }
    `;
    const module = device.createShaderModule({ code });
    return { module, code };
  }
}

/**
 * Makes a new LimitTest class so that the tests have access to `limit`
 */
function makeLimitTestFixture(limit: GPUSupportedLimit): typeof LimitTestsImpl {
  class LimitTests extends LimitTestsImpl {
    override limit = limit;
  }

  return LimitTests;
}

/**
 * This is to avoid repeating yourself (D.R.Y.) as I ran into that issue multiple times
 * writing these tests where I'd copy a test, need to rename a limit in 3-4 places,
 * forget one place, and then spend 20-30 minutes wondering why the test was failing.
 */
export function makeLimitTestGroup(limit: GPUSupportedLimit) {
  const description = `API Validation Tests for ${limit}.`;
  const g = makeTestGroup(makeLimitTestFixture(limit));
  return { g, description, limit };
}

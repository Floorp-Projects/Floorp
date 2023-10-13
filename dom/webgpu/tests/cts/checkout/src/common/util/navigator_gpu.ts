/// <reference types="@webgpu/types" />

import { TestCaseRecorder } from '../framework/fixture.js';

import { ErrorWithExtra, assert, objectEquals } from './util.js';

/**
 * Finds and returns the `navigator.gpu` object (or equivalent, for non-browser implementations).
 * Throws an exception if not found.
 */
function defaultGPUProvider(): GPU {
  assert(
    typeof navigator !== 'undefined' && navigator.gpu !== undefined,
    'No WebGPU implementation found'
  );
  return navigator.gpu;
}

/**
 * GPUProvider is a function that creates and returns a new GPU instance.
 * May throw an exception if a GPU cannot be created.
 */
export type GPUProvider = () => GPU;

let gpuProvider: GPUProvider = defaultGPUProvider;

/**
 * Sets the function to create and return a new GPU instance.
 */
export function setGPUProvider(provider: GPUProvider) {
  assert(impl === undefined, 'setGPUProvider() should not be after getGPU()');
  gpuProvider = provider;
}

let impl: GPU | undefined = undefined;

let defaultRequestAdapterOptions: GPURequestAdapterOptions | undefined;

export function setDefaultRequestAdapterOptions(options: GPURequestAdapterOptions) {
  // It's okay to call this if you don't change the options
  if (objectEquals(options, defaultRequestAdapterOptions)) {
    return;
  }
  if (impl) {
    throw new Error('must call setDefaultRequestAdapterOptions before getGPU');
  }
  defaultRequestAdapterOptions = { ...options };
}

export function getDefaultRequestAdapterOptions() {
  return defaultRequestAdapterOptions;
}

/**
 * Finds and returns the `navigator.gpu` object (or equivalent, for non-browser implementations).
 * Throws an exception if not found.
 */
export function getGPU(recorder: TestCaseRecorder | null): GPU {
  if (impl) {
    return impl;
  }

  impl = gpuProvider();

  if (defaultRequestAdapterOptions) {
    // eslint-disable-next-line @typescript-eslint/unbound-method
    const oldFn = impl.requestAdapter;
    impl.requestAdapter = function (
      options?: GPURequestAdapterOptions
    ): Promise<GPUAdapter | null> {
      const promise = oldFn.call(this, { ...defaultRequestAdapterOptions, ...options });
      if (recorder) {
        void promise.then(async adapter => {
          if (adapter) {
            const info = await adapter.requestAdapterInfo();
            const infoString = `Adapter: ${info.vendor} / ${info.architecture} / ${info.device}`;
            recorder.debug(new ErrorWithExtra(infoString, () => ({ adapterInfo: info })));
          }
        });
      }
      return promise;
    };
  }

  return impl;
}

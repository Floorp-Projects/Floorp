/// <reference types="@webgpu/types" />

import { assert } from './util.js';

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
  if (impl) {
    throw new Error('must call setDefaultRequestAdapterOptions before getGPU');
  }
  defaultRequestAdapterOptions = { ...options };
}

/**
 * Finds and returns the `navigator.gpu` object (or equivalent, for non-browser implementations).
 * Throws an exception if not found.
 */
export function getGPU(): GPU {
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
      const promise = oldFn.call(this, { ...defaultRequestAdapterOptions, ...(options || {}) });
      void promise.then(async adapter => {
        if (adapter) {
          const info = await adapter.requestAdapterInfo();
          // eslint-disable-next-line no-console
          console.log(info);
        }
      });
      return promise;
    };
  }

  return impl;
}

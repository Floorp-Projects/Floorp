import { unreachable } from '../../../../../common/util/util.js';
import { GPUTest } from '../../../../gpu_test.js';
import { EncoderType } from '../../../../util/command_buffer_maker.js';

interface BindGroupIndices {
  a: number;
  b: number;
  out: number;
}

export class ProgrammableStateTest extends GPUTest {
  private commonBindGroupLayouts: Map<string, GPUBindGroupLayout> = new Map();

  getBindGroupLayout(type: GPUBufferBindingType): GPUBindGroupLayout {
    if (!this.commonBindGroupLayouts.has(type)) {
      this.commonBindGroupLayouts.set(
        type,
        this.device.createBindGroupLayout({
          entries: [
            {
              binding: 0,
              visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
              buffer: { type },
            },
          ],
        })
      );
    }
    return this.commonBindGroupLayouts.get(type)!;
  }

  getBindGroupLayouts(indices: BindGroupIndices): GPUBindGroupLayout[] {
    const bindGroupLayouts: GPUBindGroupLayout[] = [];
    bindGroupLayouts[indices.a] = this.getBindGroupLayout('read-only-storage');
    bindGroupLayouts[indices.b] = this.getBindGroupLayout('read-only-storage');
    bindGroupLayouts[indices.out] = this.getBindGroupLayout('storage');
    return bindGroupLayouts;
  }

  createBindGroup(buffer: GPUBuffer, type: GPUBufferBindingType): GPUBindGroup {
    return this.device.createBindGroup({
      layout: this.getBindGroupLayout(type),
      entries: [{ binding: 0, resource: { buffer } }],
    });
  }

  setBindGroup(
    encoder: GPUBindingCommandsMixin,
    index: number,
    factory: (index: number) => GPUBindGroup
  ) {
    encoder.setBindGroup(index, factory(index));
  }

  // Create a compute pipeline that performs an operation on data from two bind groups,
  // then writes the result to a third bind group.
  createBindingStatePipeline<T extends EncoderType>(
    encoderType: T,
    groups: BindGroupIndices,
    algorithm: string = 'a.value - b.value'
  ): GPUComputePipeline | GPURenderPipeline {
    switch (encoderType) {
      case 'compute pass': {
        const wgsl = `struct Data {
            value : i32
          };

          @group(${groups.a}) @binding(0) var<storage> a : Data;
          @group(${groups.b}) @binding(0) var<storage> b : Data;
          @group(${groups.out}) @binding(0) var<storage, read_write> out : Data;

          @compute @workgroup_size(1) fn main() {
            out.value = ${algorithm};
            return;
          }
        `;

        return this.device.createComputePipeline({
          layout: this.device.createPipelineLayout({
            bindGroupLayouts: this.getBindGroupLayouts(groups),
          }),
          compute: {
            module: this.device.createShaderModule({
              code: wgsl,
            }),
            entryPoint: 'main',
          },
        });
      }
      case 'render pass':
      case 'render bundle': {
        const wgslShaders = {
          vertex: `
            @vertex fn vert_main() -> @builtin(position) vec4<f32> {
              return vec4<f32>(0.5, 0.5, 0.0, 1.0);
            }
          `,

          fragment: `
            struct Data {
              value : i32
            };

            @group(${groups.a}) @binding(0) var<storage> a : Data;
            @group(${groups.b}) @binding(0) var<storage> b : Data;
            @group(${groups.out}) @binding(0) var<storage, read_write> out : Data;

            @fragment fn frag_main() -> @location(0) vec4<f32> {
              out.value = ${algorithm};
              return vec4<f32>(1.0, 0.0, 0.0, 1.0);
            }
          `,
        };

        return this.device.createRenderPipeline({
          layout: this.device.createPipelineLayout({
            bindGroupLayouts: this.getBindGroupLayouts(groups),
          }),
          vertex: {
            module: this.device.createShaderModule({
              code: wgslShaders.vertex,
            }),
            entryPoint: 'vert_main',
          },
          fragment: {
            module: this.device.createShaderModule({
              code: wgslShaders.fragment,
            }),
            entryPoint: 'frag_main',
            targets: [{ format: 'rgba8unorm' }],
          },
          primitive: { topology: 'point-list' },
        });
      }
      default:
        unreachable();
    }
  }

  setPipeline(pass: GPUBindingCommandsMixin, pipeline: GPUComputePipeline | GPURenderPipeline) {
    if (pass instanceof GPUComputePassEncoder) {
      pass.setPipeline(pipeline as GPUComputePipeline);
    } else if (pass instanceof GPURenderPassEncoder || pass instanceof GPURenderBundleEncoder) {
      pass.setPipeline(pipeline as GPURenderPipeline);
    }
  }

  dispatchOrDraw(pass: GPUBindingCommandsMixin) {
    if (pass instanceof GPUComputePassEncoder) {
      pass.dispatchWorkgroups(1);
    } else if (pass instanceof GPURenderPassEncoder) {
      pass.draw(1);
    } else if (pass instanceof GPURenderBundleEncoder) {
      pass.draw(1);
    }
  }
}

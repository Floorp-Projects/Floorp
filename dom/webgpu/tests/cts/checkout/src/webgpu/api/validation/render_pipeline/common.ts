import { kTextureFormatInfo } from '../../../format_info.js';
import {
  getFragmentShaderCodeWithOutput,
  getPlainTypeInfo,
  kDefaultVertexShaderCode,
} from '../../../util/shader.js';
import { ValidationTest } from '../validation_test.js';

const values = [0, 1, 0, 1];
export class CreateRenderPipelineValidationTest extends ValidationTest {
  getDescriptor(
    options: {
      primitive?: GPUPrimitiveState;
      targets?: GPUColorTargetState[];
      multisample?: GPUMultisampleState;
      depthStencil?: GPUDepthStencilState;
      fragmentShaderCode?: string;
      noFragment?: boolean;
      fragmentConstants?: Record<string, GPUPipelineConstantValue>;
    } = {}
  ): GPURenderPipelineDescriptor {
    const defaultTargets: GPUColorTargetState[] = [{ format: 'rgba8unorm' }];
    const {
      primitive = {},
      targets = defaultTargets,
      multisample = {},
      depthStencil,
      fragmentShaderCode = getFragmentShaderCodeWithOutput([
        {
          values,
          plainType: getPlainTypeInfo(
            kTextureFormatInfo[targets[0] ? targets[0].format : 'rgba8unorm'].sampleType
          ),
          componentCount: 4,
        },
      ]),
      noFragment = false,
      fragmentConstants = {},
    } = options;

    return {
      vertex: {
        module: this.device.createShaderModule({
          code: kDefaultVertexShaderCode,
        }),
        entryPoint: 'main',
      },
      fragment: noFragment
        ? undefined
        : {
            module: this.device.createShaderModule({
              code: fragmentShaderCode,
            }),
            entryPoint: 'main',
            targets,
            constants: fragmentConstants,
          },
      layout: this.getPipelineLayout(),
      primitive,
      multisample,
      depthStencil,
    };
  }

  getPipelineLayout(): GPUPipelineLayout {
    return this.device.createPipelineLayout({ bindGroupLayouts: [] });
  }
}

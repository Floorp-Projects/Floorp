# Copyright (c) 2010 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'essl_to_glsl',
      'type': 'executable',
      'dependencies': [
        '../src/build_angle.gyp:translator_glsl',
      ],
      'include_dirs': [
        '../include',
      ],
      'sources': [
        'translator/translator.cpp',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'essl_to_hlsl',
          'type': 'executable',
          'dependencies': [
            '../src/build_angle.gyp:translator_hlsl',
          ],
          'include_dirs': [
            '../include',
            '../src',
          ],
          'sources': [
            'translator/translator.cpp',
            '../src/common/debug.cpp',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalLibraryDirectories': ['$(DXSDK_DIR)/lib/x86'],
              'AdditionalDependencies': ['d3d9.lib'],
            }
          }
        },
        {
          'target_name': 'es_util',
          'type': 'static_library',
          'dependencies': [
            '../src/build_angle.gyp:libEGL',
            '../src/build_angle.gyp:libGLESv2',
          ],
          'include_dirs': [
            'gles2_book/Common',
            '../include',
          ],
          'sources': [
            'gles2_book/Common/esShader.c',
            'gles2_book/Common/esShapes.c',
            'gles2_book/Common/esTransform.c',
            'gles2_book/Common/esUtil.c',
            'gles2_book/Common/esUtil.h',
            'gles2_book/Common/esUtil_win.h',
            'gles2_book/Common/Win32/esUtil_TGA.c',
            'gles2_book/Common/Win32/esUtil_win32.c',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'gles2_book/Common',
              '../include',
            ],
          },
        },
        {
          'target_name': 'hello_triangle',
          'type': 'executable',
          'dependencies': ['es_util'],
          'sources': [
            'gles2_book/Hello_Triangle/Hello_Triangle.c',
          ],
        },
        {
          'target_name': 'mip_map_2d',
          'type': 'executable',
          'dependencies': ['es_util'],
          'sources': [
            'gles2_book/MipMap2D/MipMap2D.c',
          ],
        },
        {
          'target_name': 'multi_texture',
          'type': 'executable',
          'dependencies': ['es_util'],
          'sources': [
            'gles2_book/MultiTexture/MultiTexture.c',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                'gles2_book/MultiTexture/basemap.tga',
                'gles2_book/MultiTexture/lightmap.tga',
              ],
            },
          ],
        },
        {
          'target_name': 'particle_system',
          'type': 'executable',
          'dependencies': ['es_util'],
          'sources': [
            'gles2_book/ParticleSystem/ParticleSystem.c',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                'gles2_book/ParticleSystem/smoke.tga',
              ],
            },
          ],
        },
        {
          'target_name': 'simple_texture_2d',
          'type': 'executable',
          'dependencies': ['es_util'],
          'sources': [
            'gles2_book/Simple_Texture2D/Simple_Texture2D.c',
          ],
        },
        {
          'target_name': 'simple_texture_cubemap',
          'type': 'executable',
          'dependencies': ['es_util'],
          'sources': [
            'gles2_book/Simple_TextureCubemap/Simple_TextureCubemap.c',
          ],
        },
        {
          'target_name': 'simple_vertex_shader',
          'type': 'executable',
          'dependencies': ['es_util'],
          'sources': [
            'gles2_book/Simple_VertexShader/Simple_VertexShader.c',
          ],
        },
        {
          'target_name': 'stencil_test',
          'type': 'executable',
          'dependencies': ['es_util'],
          'sources': [
            'gles2_book/Stencil_Test/Stencil_Test.c',
          ],
        },
        {
          'target_name': 'texture_wrap',
          'type': 'executable',
          'dependencies': ['es_util'],
          'sources': [
            'gles2_book/TextureWrap/TextureWrap.c',
          ],
        },
        {
          'target_name': 'post_sub_buffer',
          'type': 'executable',
          'dependencies': ['es_util'],
          'sources': [
            'gles2_book/PostSubBuffer/PostSubBuffer.c',
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

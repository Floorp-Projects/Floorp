/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This IDL depends on WebGLRenderingContext.webidl
 */

[Pref="webgl.enable-prototype-webgl2"]
interface WebGLQuery {
};

[Pref="webgl.enable-prototype-webgl2"]
interface WebGL2RenderingContext : WebGLRenderingContext {

    /* draw buffers */
    const GLenum COLOR_ATTACHMENT1                           = 0x8CE1;
    const GLenum COLOR_ATTACHMENT2                           = 0x8CE2;
    const GLenum COLOR_ATTACHMENT3                           = 0x8CE3;
    const GLenum COLOR_ATTACHMENT4                           = 0x8CE4;
    const GLenum COLOR_ATTACHMENT5                           = 0x8CE5;
    const GLenum COLOR_ATTACHMENT6                           = 0x8CE6;
    const GLenum COLOR_ATTACHMENT7                           = 0x8CE7;
    const GLenum COLOR_ATTACHMENT8                           = 0x8CE8;
    const GLenum COLOR_ATTACHMENT9                           = 0x8CE9;
    const GLenum COLOR_ATTACHMENT10                          = 0x8CEA;
    const GLenum COLOR_ATTACHMENT11                          = 0x8CEB;
    const GLenum COLOR_ATTACHMENT12                          = 0x8CEC;
    const GLenum COLOR_ATTACHMENT13                          = 0x8CED;
    const GLenum COLOR_ATTACHMENT14                          = 0x8CEE;
    const GLenum COLOR_ATTACHMENT15                          = 0x8CEF;

    const GLenum DRAW_BUFFER0                                = 0x8825;
    const GLenum DRAW_BUFFER1                                = 0x8826;
    const GLenum DRAW_BUFFER2                                = 0x8827;
    const GLenum DRAW_BUFFER3                                = 0x8828;
    const GLenum DRAW_BUFFER4                                = 0x8829;
    const GLenum DRAW_BUFFER5                                = 0x882A;
    const GLenum DRAW_BUFFER6                                = 0x882B;
    const GLenum DRAW_BUFFER7                                = 0x882C;
    const GLenum DRAW_BUFFER8                                = 0x882D;
    const GLenum DRAW_BUFFER9                                = 0x882E;
    const GLenum DRAW_BUFFER10                               = 0x882F;
    const GLenum DRAW_BUFFER11                               = 0x8830;
    const GLenum DRAW_BUFFER12                               = 0x8831;
    const GLenum DRAW_BUFFER13                               = 0x8832;
    const GLenum DRAW_BUFFER14                               = 0x8833;
    const GLenum DRAW_BUFFER15                               = 0x8834;

    const GLenum MAX_COLOR_ATTACHMENTS                       = 0x8CDF;
    const GLenum MAX_DRAW_BUFFERS                            = 0x8824;

    void drawBuffers(sequence<GLenum> buffers);


    /* draw instanced */
    void drawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
    void drawElementsInstanced(GLenum mode, GLsizei count, GLenum type, GLintptr offset, GLsizei primcount);


    /* instanced array */
    const GLenum VERTEX_ATTRIB_ARRAY_DIVISOR                 = 0x88FE;

    void vertexAttribDivisor(GLuint index, GLuint divisor);


    /* blend equations */
    const GLenum MIN                                         = 0x8007;
    const GLenum MAX                                         = 0x8008;


    /* transform feedback */
    const GLenum RASTERIZER_DISCARD                          = 0x8C89;
    const GLenum TRANSFORM_FEEDBACK_BUFFER                   = 0x8C8E;
    const GLenum TRANSFORM_FEEDBACK_BUFFER_BINDING           = 0x8C8F;
    const GLenum TRANSFORM_FEEDBACK_BUFFER_START             = 0x8C84;
    const GLenum TRANSFORM_FEEDBACK_BUFFER_SIZE              = 0x8C85;
    const GLenum MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS     = 0x8C8B;


    /* query objects */
    const GLenum CURRENT_QUERY                               = 0x8865;
    const GLenum QUERY_RESULT                                = 0x8866;
    const GLenum QUERY_RESULT_AVAILABLE                      = 0x8867;

    void beginQuery(GLenum target, WebGLQuery? queryObject);
    WebGLQuery? createQuery();
    void deleteQuery(WebGLQuery? queryObject);
    void endQuery(GLenum target);
    WebGLQuery? getQuery(GLenum target, GLenum pname);
    any getQueryObject(WebGLQuery? queryObject, GLenum pname);
    [WebGLHandlesContextLoss] GLboolean isQuery(WebGLQuery? queryObject);


    /* occlusion queries */
    const GLenum ANY_SAMPLES_PASSED                          = 0x8C2F;
    const GLenum ANY_SAMPLES_PASSED_CONSERVATIVE             = 0x8D6A;


    /* transform feedback queries */
    const GLenum TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN       = 0x8C88;


    /* buffer objects */
    void bindBufferBase(GLenum target, GLuint index, WebGLBuffer? buffer);
    void bindBufferRange(GLenum target, GLuint index, WebGLBuffer? buffer,
                         GLintptr offset, GLsizeiptr size);


    /* state requests */
    any getParameterIndexed(GLenum pname, GLuint index);


    /* vertex array objects */
    const GLenum VERTEX_ARRAY_BINDING                        = 0x85B5;

    void bindVertexArray(WebGLVertexArray? arrayObject);
    WebGLVertexArray? createVertexArray();
    void deleteVertexArray(WebGLVertexArray? arrayObject);
    [WebGLHandlesContextLoss] GLboolean isVertexArray(WebGLVertexArray? arrayObject);

};


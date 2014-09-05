precision mediump float;

varying vec4 vColor;
uniform sampler2D uSampler;
varying vec2 vCoordinate;

void main() {
  // gl_FragColor = vColor;
  // gl_FragColor = vec4(vTextureCoordinate.x, vTextureCoordinate.y, 0, 0.5);
  // gl_FragColor = gl_FragColor; // + texture2D(uSampler, vCoordinate);
  gl_FragColor = texture2D(uSampler, vCoordinate);
}
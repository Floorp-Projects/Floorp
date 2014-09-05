precision mediump float;

varying vec4 vColor;
uniform sampler2D uSampler;
varying vec2 vCoordinate;

void main() {
  const int sampleRadius = 8;
  const int samples = sampleRadius * 2 + 1;
  float dx = 0.01;
  vec4 sample = vec4(0, 0, 0, 0);
  for (int i = -sampleRadius; i <= sampleRadius; i++) {
    sample += texture2D(uSampler, vCoordinate + vec2(0, float(i) * dy));
  }
  gl_FragColor = sample / float(samples);
}
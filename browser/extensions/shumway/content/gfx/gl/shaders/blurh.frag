/*
precision mediump float;

varying vec4 vColor;
uniform sampler2D uSampler;
varying vec2 vCoordinate;

out vec4 FragmentColor;

uniform float offset[5] = float[]( 0.0, 1.0, 2.0, 3.0, 4.0 );
uniform float weight[5] = float[]( 0.2270270270, 0.1945945946, 0.1216216216,
                                   0.0540540541, 0.0162162162 );

void main(void)
{
    FragmentColor = texture2D( uSampler, vec2(vCoordinate) * weight[0];
    for (int i=1; i<5; i++) {
        FragmentColor +=
            texture2D( uSampler, ( vec2(gl_FragCoord)+vec2(0.0, offset[i]) )/1024.0 )
                * weight[i];
        FragmentColor +=
            texture2D( uSampler, ( vec2(gl_FragCoord)-vec2(0.0, offset[i]) )/1024.0 )
                * weight[i];
    }
}
*/

/*
precision mediump float;

varying vec4 vColor;
uniform sampler2D uSampler;
varying vec2 vCoordinate;

void main() {
  const int sampleRadius = 16;
  const int samples = sampleRadius * 2 + 1;
  float dy = 1.0 / 512.0;
  vec4 sample = vec4(0, 0, 0, 0);
  for (int i = -sampleRadius; i <= sampleRadius; i++) {
    sample += texture2D(uSampler, vCoordinate + vec2(0, float(i) * dy));
  }
  gl_FragColor = sample / float(samples);
  // gl_FragColor = texture2D(uSampler, vCoordinate);
}
*/

precision mediump float;

varying vec4 vColor;
uniform sampler2D uSampler;
varying vec2 vCoordinate;

void main() {
  vec4 sum = vec4(0.0);
  float blur = 1.0 / 512.0 * 1.0;
  sum += texture2D(uSampler, vec2(vCoordinate.x - 4.0 * blur, vCoordinate.y)) * 0.05;
  sum += texture2D(uSampler, vec2(vCoordinate.x - 3.0 * blur, vCoordinate.y)) * 0.09;
  sum += texture2D(uSampler, vec2(vCoordinate.x - 2.0 * blur, vCoordinate.y)) * 0.12;
  sum += texture2D(uSampler, vec2(vCoordinate.x - blur, vCoordinate.y)) * 0.15;
  sum += texture2D(uSampler, vec2(vCoordinate.x, vCoordinate.y)) * 0.16;
  sum += texture2D(uSampler, vec2(vCoordinate.x + blur, vCoordinate.y)) * 0.15;
  sum += texture2D(uSampler, vec2(vCoordinate.x + 2.0 * blur, vCoordinate.y)) * 0.12;
  sum += texture2D(uSampler, vec2(vCoordinate.x + 3.0 * blur, vCoordinate.y)) * 0.09;
  sum += texture2D(uSampler, vec2(vCoordinate.x + 4.0 * blur, vCoordinate.y)) * 0.05;
  gl_FragColor = sum;
  // gl_FragColor = texture2D(uSampler, vCoordinate);
}

/*
precision mediump float;

varying vec4 vColor;
uniform sampler2D uSampler;
varying vec2 vCoordinate;

void main() {
  vec4 sum = vec4(0.0);
  float blur = 0.1;
  sum += texture2D(uSampler, vec2(vCoordinate.x - 4.0 * blur, vCoordinate.y)) * 0.05;
  sum += texture2D(uSampler, vec2(vCoordinate.x - 3.0 * blur, vCoordinate.y)) * 0.09;
  sum += texture2D(uSampler, vec2(vCoordinate.x - 2.0 * blur, vCoordinate.y)) * 0.12;
  sum += texture2D(uSampler, vec2(vCoordinate.x - blur, vCoordinate.y)) * 0.15;
  sum += texture2D(uSampler, vec2(vCoordinate.x, vCoordinate.y)) * 0.16;
  sum += texture2D(uSampler, vec2(vCoordinate.x + blur, vCoordinate.y)) * 0.15;
  sum += texture2D(uSampler, vec2(vCoordinate.x + 2.0 * blur, vCoordinate.y)) * 0.12;
  sum += texture2D(uSampler, vec2(vCoordinate.x + 3.0 * blur, vCoordinate.y)) * 0.09;
  sum += texture2D(uSampler, vec2(vCoordinate.x + 4.0 * blur, vCoordinate.y)) * 0.05;
  gl_FragColor = sum;
  // gl_FragColor = texture2D(uSampler, vCoordinate);
}
*/

/*
precision mediump float;

varying vec4 vColor;
uniform sampler2D uSampler;
varying vec2 vCoordinate;

void main() {
  gl_FragColor = texture2D(uSampler, vCoordinate);
}

*/

/*
precision mediump float;
varying vec2 vCoordinate;
varying float vColor;
uniform float blur;
uniform sampler2D uSampler;
void main(void) {
vec4 sum = vec4(0.0);
sum += texture2D(uSampler, vec2(vCoordinate.x - 4.0*blur, vCoordinate.y)) * 0.05;
sum += texture2D(uSampler, vec2(vCoordinate.x - 3.0*blur, vCoordinate.y)) * 0.09;
sum += texture2D(uSampler, vec2(vCoordinate.x - 2.0*blur, vCoordinate.y)) * 0.12;
sum += texture2D(uSampler, vec2(vCoordinate.x - blur, vCoordinate.y)) * 0.15;
sum += texture2D(uSampler, vec2(vCoordinate.x, vCoordinate.y)) * 0.16;
sum += texture2D(uSampler, vec2(vCoordinate.x + blur, vCoordinate.y)) * 0.15;
sum += texture2D(uSampler, vec2(vCoordinate.x + 2.0*blur, vCoordinate.y)) * 0.12;
sum += texture2D(uSampler, vec2(vCoordinate.x + 3.0*blur, vCoordinate.y)) * 0.09;
sum += texture2D(uSampler, vec2(vCoordinate.x + 4.0*blur, vCoordinate.y)) * 0.05;
gl_FragColor = sum;
}

*/
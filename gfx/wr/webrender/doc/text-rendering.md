# Text Rendering

This document describes the details of how WebRender renders text, particularly the blending stage of text rendering.
We will go into grayscale text blending, subpixel text blending, and "subpixel text with background color" blending.

### Prerequisites

The description below assumes you're familiar with regular rgba compositing, operator over,
and the concept of premultiplied alpha.

### Not covered in this document

We are going to treat the origin of the text mask as a black box.
We're also going to assume we can blend text in the device color space and will not go into the gamma correction and linear pre-blending that happens in some of the backends that produce the text masks.

## Grayscale Text Blending

Grayscale text blending is the simplest form of text blending. Our blending function has three inputs:

 - The text color, as a premultiplied rgba color.
 - The text mask, as a single-channel alpha texture.
 - The existing contents of the framebuffer that we're rendering to, the "destination". This is also a premultiplied rgba buffer.

Note: The word "grayscale" here does *not* mean that we can only draw gray text.
It means that the mask only has a single alpha value per pixel, so we can visualize
the mask in our minds as a grayscale image.

### Deriving the math

We want to mask our text color using the single-channel mask, and composite that to the destination.
This compositing step uses operator "over", just like regular compositing of rgba images.

I'll be using GLSL syntax to describe the blend equations, but please consider most of the code below pseudocode.

We can express the blending described above as the following blend equation:

```glsl
vec4 textblend(vec4 text_color, vec4 mask, vec4 dest) {
  return over(in(text_color, mask), dest);
}
```

with `over` being the blend function for (premultiplied) operator "over":

```glsl
vec4 over(vec4 src, vec4 dest) {
  return src + (1.0 - src.a) * dest;
}
```

and `in` being the blend function for (premultiplied) operator "in", i.e. the masking operator:

```glsl
vec4 in(vec4 src, vec4 mask) {
  return src * mask.a;
}
```

So the complete blending function is:

```glsl
result.r = text_color.r * mask.a + (1.0 - text_color.a * mask.a) * dest.r;
result.g = text_color.g * mask.a + (1.0 - text_color.a * mask.a) * dest.g;
result.b = text_color.b * mask.a + (1.0 - text_color.a * mask.a) * dest.b;
result.a = text_color.a * mask.a + (1.0 - text_color.a * mask.a) * dest.a;
```

### Rendering this with OpenGL

In general, a fragment shader does not have access to the destination.
So the full blend equation needs to be expressed in a way that the shader only computes values that are independent of the destination,
and the parts of the equation that use the destination values need to be applied by the OpenGL blend pipeline itself.
The OpenGL blend pipeline can be tweaked using the functions `glBlendEquation` and `glBlendFunc`.

In our example, the fragment shader can output just `text_color * mask.a`:

```glsl
  oFragColor = text_color * mask.a;
```

and the OpenGL blend pipeline can be configured like so:

```rust
    pub fn set_blend_mode_premultiplied_alpha(&self) {
        self.gl.blend_func(gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
```

This results in an overall blend equation of

```
result.r = 1 * oFragColor.r + (1 - oFragColor.a) * dest.r;
           ^                ^  ^^^^^^^^^^^^^^^^^
           |                |         |
           +--gl::ONE       |         +-- gl::ONE_MINUS_SRC_ALPHA
                            |
                            +-- gl::FUNC_ADD

         = 1 * (text_color.r * mask.a) + (1 - (text_color.a * mask.a)) * dest.r
         = text_color.r * mask.a + (1 - text_color.a * mask.a) * dest.r
```

which is exactly what we wanted.

### Differences to the actual WebRender code

There are two minor differences between the shader code above and the actual code in the text run shader in WebRender:

```glsl
oFragColor = text_color * mask.a;    // (shown above)
// vs.
oFragColor = vColor * mask * alpha;  // (actual webrender code)
```

`vColor` is set to the text color. The differences are:

 - WebRender multiplies with all components of `mask` instead of just with `mask.a`.
   However, our font rasterization code fills the rgb values of `mask` with the value of `mask.a`,
   so this is completely equivalent.
 - WebRender applies another alpha to the text. This is coming from the clip.
   You can think of this alpha to be a pre-adjustment of the text color for that pixel, or as an
   additional mask that gets applied to the mask.

## Subpixel Text Blending

Now that we have the blend equation for single-channel text blending, we can look at subpixel text blending.

The main difference between subpixel text blending and grayscale text blending is the fact that,
for subpixel text, the text mask contains a separate alpha value for each color component.

### Component alpha

Regular painting uses four values per pixel: three color values, and one alpha value. The alpha value applies to all components of the pixel equally.

Imagine for a second a world in which you have *three alpha values per pixel*, one for each color component.

 - Old world: Each pixel has four values: `color.r`, `color.g`, `color.b`, and `color.a`.
 - New world: Each pixel has *six* values: `color.r`, `color.a_r`, `color.g`, `color.a_g`, `color.b`, and `color.a_b`.

In such a world we can define a component-alpha-aware operator "over":

```glsl
vec6 over_comp(vec6 src, vec6 dest) {
  vec6 result;
  result.r = src.r + (1.0 - src.a_r) * dest.r;
  result.g = src.g + (1.0 - src.a_g) * dest.g;
  result.b = src.b + (1.0 - src.a_b) * dest.b;
  result.a_r = src.a_r + (1.0 - src.a_r) * dest.a_r;
  result.a_g = src.a_g + (1.0 - src.a_g) * dest.a_g;
  result.a_b = src.a_b + (1.0 - src.a_b) * dest.a_b;
  return result;
}
```

and a component-alpha-aware operator "in":

```glsl
vec6 in_comp(vec6 src, vec6 mask) {
  vec6 result;
  result.r = src.r * mask.a_r;
  result.g = src.g * mask.a_g;
  result.b = src.b * mask.a_b;
  result.a_r = src.a_r * mask.a_r;
  result.a_g = src.a_g * mask.a_g;
  result.a_b = src.a_b * mask.a_b;
  return result;
}
```

and even a component-alpha-aware version of `textblend`:

```glsl
vec6 textblend_comp(vec6 text_color, vec6 mask, vec6 dest) {
  return over_comp(in_comp(text_color, mask), dest);
}
```

This results in the following set of equations:

```glsl
result.r = text_color.r * mask.a_r + (1.0 - text_color.a_r * mask.a_r) * dest.r;
result.g = text_color.g * mask.a_g + (1.0 - text_color.a_g * mask.a_g) * dest.g;
result.b = text_color.b * mask.a_b + (1.0 - text_color.a_b * mask.a_b) * dest.b;
result.a_r = text_color.a_r * mask.a_r + (1.0 - text_color.a_r * mask.a_r) * dest.a_r;
result.a_g = text_color.a_g * mask.a_g + (1.0 - text_color.a_g * mask.a_g) * dest.a_g;
result.a_b = text_color.a_b * mask.a_b + (1.0 - text_color.a_b * mask.a_b) * dest.a_b;
```

### Back to the real world

If we want to transfer the component alpha blend equation into the real world, we need to make a few small changes:

 - Our text color only needs one alpha value.
   So we'll replace all instances of `text_color.a_r/g/b` with `text_color.a`.
 - We're currently not making use of the mask's `r`, `g` and `b` values, only of the `a_r`, `a_g` and `a_b` values.
   So in the real world, we can use the rgb channels of `mask` to store those component alphas and
   replace `mask.a_r/g/b` with `mask.r/g/b`.

These two changes give us:

```glsl
result.r = text_color.r * mask.r + (1.0 - text_color.a * mask.r) * dest.r;
result.g = text_color.g * mask.g + (1.0 - text_color.a * mask.g) * dest.g;
result.b = text_color.b * mask.b + (1.0 - text_color.a * mask.b) * dest.b;
result.a_r = text_color.a * mask.r + (1.0 - text_color.a * mask.r) * dest.a_r;
result.a_g = text_color.a * mask.g + (1.0 - text_color.a * mask.g) * dest.a_g;
result.a_b = text_color.a * mask.b + (1.0 - text_color.a * mask.b) * dest.a_b;
```

There's a third change we need to make:

 - We're rendering to a destination surface that only has one alpha channel instead of three.
   So `dest.a_r/g/b` and `result.a_r/g/b` will need to become `dest.a` and `result.a`.

This creates a problem: We're currently assigning different values to `result.a_r`, `result.a_g` and `result.a_b`.
Which of them should we use to compute `result.a`?

This question does not have an answer. One alpha value per pixel is simply not sufficient
to express the same information as three alpha values.

However, see what happens if the destination is already opaque:

We have `dest.a_r == 1`, `dest.a_g == 1`, and `dest.a_b == 1`.

```
result.a_r = text_color.a * mask.r + (1 - text_color.a * mask.r) * dest.a_r
           = text_color.a * mask.r + (1 - text_color.a * mask.r) * 1
           = text_color.a * mask.r + 1 - text_color.a * mask.r
           = 1
same for result.a_g and result.a_b
```

In other words, for opaque destinations, it doesn't matter what which channel of the mask we use when computing `result.a`, the result will always be completely opaque anyways. In WebRender we just pick `mask.g` (or rather,
have font rasterization set `mask.a` to the value of `mask.g`) because it's as good as any.

The takeaway here is: **Subpixel text blending is only supported for opaque destinations.** Attempting to render subpixel
text into partially transparent destinations will result in bad alpha values. Or rather, it will result in alpha values which
are not anticipated by the r, g, and b values in the same pixel, so that subsequent blend operations, which will mix r and a values
from the same pixel, will produce incorrect colors.

Here's the final subpixel blend function:

```glsl
vec4 subpixeltextblend(vec4 text_color, vec4 mask, vec4 dest) {
  vec4 result;
  result.r = text_color.r * mask.r + (1.0 - text_color.a * mask.r) * dest.r;
  result.g = text_color.g * mask.g + (1.0 - text_color.a * mask.g) * dest.g;
  result.b = text_color.b * mask.b + (1.0 - text_color.a * mask.b) * dest.b;
  result.a = text_color.a * mask.a + (1.0 - text_color.a * mask.a) * dest.a;
  return result;
}
```

or for short:

```glsl
vec4 subpixeltextblend(vec4 text_color, vec4 mask, vec4 dest) {
  return text_color * mask + (1.0 - text_color.a * mask) * dest;
}
```

To recap, here's what we gained and lost by making the transition from the full-component-alpha world to the
regular rgba world: All colors and textures now only need four values to be represented, we still use a
component alpha mask, and the results are equivalent to the full-component-alpha result assuming that the
destination is opaque. We lost the ability to draw to partially transparent destinations.

### Making this work in OpenGL

We have the complete subpixel blend function.
Now we need to cut it into pieces and mix it with the OpenGL blend pipeline in such a way that
the fragment shader does not need to know about the destination.

Compare the equation for the red channel and the alpha channel between the two ways of text blending:

```
  single-channel alpha:
    result.r = text_color.r * mask.a + (1.0 - text_color.a * mask.a) * dest.r
    result.a = text_color.a * mask.a + (1.0 - text_color.a * mask.a) * dest.r

  component alpha:
    result.r = text_color.r * mask.r + (1.0 - text_color.a * mask.r) * dest.r
    result.a = text_color.a * mask.a + (1.0 - text_color.a * mask.a) * dest.r
```

Notably, in the single-channel alpha case, all three destination color channels are multiplied with the same thing:
`(1.0 - text_color.a * mask.a)`. This factor also happens to be "one minus `oFragColor.a`".
So we were able to take advantage of OpenGL's `ONE_MINUS_SRC_ALPHA` blend func.

In the component alpha case, we're not so lucky: Each destination color channel
is multiplied with a different factor. We can use `ONE_MINUS_SRC_COLOR` instead,
and output `text_color.a * mask` from our fragment shader.
But then there's still the problem that the first summand of the computation for `result.r` uses
`text_color.r * mask.r` and the second summand uses `text_color.a * mask.r`.

There are multiple ways to deal with this. They are:

 1. Making use of `glBlendColor` and the `GL_CONSTANT_COLOR` blend func.
 2. Using a two-pass method.
 3. Using "dual source blending".

Let's look at them in order.

#### 1. Subpixel text blending in OpenGL using `glBlendColor`

In this approach we return `text_color.a * mask` from the shader.
Then we set the blend color to `text_color / text_color.a` and use `GL_CONSTANT_COLOR` as the source blendfunc.
This results in the following blend equation:

```
result.r = (text_color.r / text_color.a) * oFragColor.r + (1 - oFragColor.r) * dest.r;
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^                ^  ^^^^^^^^^^^^^^^^^
                         |                              |      |
                         +--gl::CONSTANT_COLOR          |      +-- gl::ONE_MINUS_SRC_COLOR
                                                        |
                                                        +-- gl::FUNC_ADD

         = (text_color.r / text_color.a) * (text_color.a * mask.r) + (1 - (text_color.a * mask.r)) * dest.r
         = text_color.r * mask.r + (1 - text_color.a * mask.r) * dest.r
```

At the very beginning of this document, we defined `text_color` as the *premultiplied* text color.
So instead of actually doing the calculation `text_color.r / text_color.a` when specifying the blend color,
we really just want to use the *unpremultiplied* text color in that place.
That's usually the representation we start with anyway.

#### 2. Two-pass subpixel blending in OpenGL

The `glBlendColor` method has the disadvantage that the text color is part of the OpenGL state.
So if we want to draw text with different colors, we have two use separate batches / draw calls
to draw the differently-colored parts of text.

Alternatively, we can use a two-pass method which avoids the need to use the `GL_CONSTANT_COLOR` blend func:

 - The first pass outputs `text_color.a * mask` from the fragment shader and
   uses `gl::ZERO, gl::ONE_MINUS_SRC_COLOR` as the glBlendFuncs. This achieves:

```
oFragColor = text_color.a * mask;

result_after_pass0.r = 0 * oFragColor.r + (1 - oFragColor.r) * dest.r
                     = (1 - text_color.a * mask.r) * dest.r

result_after_pass0.g = 0 * oFragColor.g + (1 - oFragColor.g) * dest.r
                     = (1 - text_color.a * mask.r) * dest.r

...
```

 - The second pass outputs `text_color * mask` from the fragment shader and uses
   `gl::ONE, gl::ONE` as the glBlendFuncs. This results in the correct overall blend equation.

```
oFragColor = text_color * mask;

result_after_pass1.r
 = 1 * oFragColor.r + 1 * result_after_pass0.r
 = text_color.r * mask.r + result_after_pass0.r
 = text_color.r * mask.r + (1 - text_color.a * mask.r) * dest.r
```

#### 3. Dual source subpixel blending in OpenGL

The third approach is similar to the second approach, but makes use of the [`ARB_blend_func_extended`](https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_blend_func_extended.txt) extension
in order to fold the two passes into one:
Instead of outputting the two different colors in two separate passes, we output them from the same pass,
as two separate fragment shader outputs.
Those outputs can then be treated as two different sources in the blend equation.

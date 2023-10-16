# Floating Point Primer

This document is meant to be a primer of the concepts related to floating point
numbers that are needed to be understood when working on tests in WebGPU's CTS.

WebGPU's CTS is responsible for testing if implementations of WebGPU are
conformant to the spec, and thus interoperable with each other.

Floating point math makes up a significant portion of the WGSL spec, and has
many subtle corner cases to get correct.

Additionally, floating point math, unlike integer math, is broadly not exact, so
how inaccurate a calculation is allowed to be is required to be stated in the
spec and tested in the CTS, as opposed to testing for a singular correct
response.

Thus, the WebGPU CTS has a significant amount of machinery around how to
correctly test floating point expectations in a fluent manner.

## Floating Point Numbers

For the context of this discussion floating point numbers, fp for short, are
single precision IEEE floating point numbers, f32 for short.

Details of how this format works are discussed as needed below, but for a more
involved discussion, please see the references in the Resources sections.

Additionally, in the Appendix there is a table of interesting/common values that
are often referenced in tests or this document.

*In the future support for f16 and abstract floats will be added to the CTS, and
this document will need to be updated.*

Floating point numbers are effectively lossy compression of the infinite number
of possible values over their range down to 32-bits of distinct points.

This means that not all numbers in the range can be exactly represented as a f32.

For example, the integer `1` is exactly represented as `0x3f800000`, but the next
nearest number `0x3f800001` is `1.00000011920928955`.

So any number between `1` and `1.00000011920928955` is not exactly represented
as a f32 and instead is approximated as either `1` or `1.00000011920928955`.

When a number X is not exactly represented by a f32 value, there are normally
two neighbouring numbers that could reasonably represent X: the nearest f32
value above X, and the nearest f32 value below X. Which of these values gets
used is dictated by the rounding mode being used, which may be something like
always round towards 0 or go to the nearest neighbour, or something else
entirely.

The process of converting numbers between precisions, like non-f32 to f32, is
called quantization. WGSL does not prescribe a specific rounding mode when
quantizing, so either of the neighbouring values is considered valid
when converting a non-exactly representable value to f32. This has significant
implications on the CTS that are discussed later.

From here on, we assume you are familiar with the internal structure of a f32
value: a sign bit, a biased exponent, and a mantissa. For reference, see
[float32 on Wikipedia](https://en.wikipedia.org/wiki/Single-precision_floating-point_format)

In the f32 format as described above, there are two possible zero values, one
with all bits being 0, called positive zero, and one all the same except with
the sign bit being 1, called negative zero.

For WGSL, and thus the CTS's purposes, these values are considered equivalent.
Typescript, which the CTS is written in, treats all zeros as positive zeros,
unless you explicitly escape hatch to differentiate between them, so most of the
time there being two zeros doesn't materially affect code.

### Normals

Normal numbers are floating point numbers whose biased exponent is not all 0s or
all 1s. For WGSL these numbers behave as you expect for floating point values
with no interesting caveats.

### Subnormals

Subnormal numbers are numbers whose biased exponent is all 0s, also called
denorms.

These are the closest numbers to zero, both positive and negative, and fill in
the gap between the normal numbers with smallest magnitude, and 0.

Some devices, for performance reasons, do not handle operations on the
subnormal numbers, and instead treat them as being zero, this is called *flush
to zero* or FTZ behaviour.

This means in the CTS that when a subnormal number is consumed or produced by an
operation, an implementation may choose to replace it with zero.

Like the rounding mode for quantization, this adds significant complexity to the
CTS, which will be discussed later.

### Inf & NaNs

Floating point numbers include positive and negative infinity to represent
values that are out of the bounds supported by the current precision.

Implementations may assume that infinities are not present. When an evaluation
would produce an infinity, an undefined value is produced instead.

Additionally, when a calculation would produce a finite value outside the
bounds of the current precision, the implementation may convert that value to
either an infinity with same sign, or the min/max representable value as
appropriate.

The CTS encodes the least restrictive interpretation of the rules in the spec,
i.e. assuming someone has made a slightly adversarial implementation that always
chooses the thing with the least accuracy.

This means that the above rules about infinities combine to say that any time an
out of bounds value is seen, any finite value is acceptable afterwards.

This is because the out of bounds value may be converted to an infinity and then
an undefined value can be used instead of the infinity.

This is actually a significant boon for the CTS implementation, because it short
circuits a bunch of complexity about clamping to edge values and handling
infinities.

Signaling NaNs are treated as quiet NaNs in the WGSL spec. And quiet NaNs have
the same "may-convert-to-undefined-value" behaviour that infinities have, so for
the purpose of the CTS they are handled by the infinite/out of bounds logic
normally.

## Notation/Terminology

When discussing floating point values in the CTS, there are a few terms used
with precise meanings, which will be elaborated here.

Additionally, any specific notation used will be specified here to avoid
confusion.

### Operations

The CTS tests for the proper execution of f32 builtins, i.e. sin, sqrt, abs,
etc, and expressions, i.e. *, /, <, etc. These collectively can be referred to
as f32 operations.

Operations, which can be thought of as mathematical functions, are mappings from
a set of inputs to a set of outputs.

Denoted `f(x, y) = X`, where f is a placeholder or the name of the operation,
lower case variables are the inputs to the function, and uppercase variables are
the outputs of the function.

Operations have one or more inputs and an output. Being a f32 operation means
that the primary space for input and output values is f32, but there is some
flexibility in this definition. For example operations with values being
restricted to a subset of integers that are representable as f32 are often
referred to as being f32 based.

Values are generally floats, integers, booleans, vector, and matrices. Consult
the WGSL spec for the exact list of types and their definitions.

For composite outputs where there are multiple values being returned, there is a
single result value made of structured data. Whereas inputs handle this by
having multiple input parameters.

Some examples of different types of operations:

`multiplication(x, y) = X`, which represents the WGSL expression `x * y`, takes
in f32 values, `x` and `y`, and produces a f32 value `X`.

`lessThen(x, y) = X`, which represents the WGSL expression `x < y`, again takes
in f32 values, but in this case returns a boolean value.

`ldexp(x, y) = X`, which builds a f32 takes, takes in a f32 values `x` and a
restricted integer `y`.

### Domain, Range, and Intervals

For an operation `f(x) = X`, the interval of valid values for the input, `x`, is
called the *domain*, and the interval for valid results, `X`, is called the
*range*.

An interval, `[a, b]`, is a set of real numbers that contains  `a`, `b`, and all
the real numbers between them.

Open-ended intervals, i.e. ones that don't include `a` and/or `b`, are avoided,
and are called out explicitly when they occur.

The convention in this doc and the CTS code is that `a <= b`, so `a` can be
referred to as the beginning of the interval and `b` as the end of the interval.

When talking about intervals, this doc and the code endeavours to avoid using
the term **range** to refer to the span of values that an interval covers,
instead using the term bounds to avoid confusion of terminology around output of
operations.

## Accuracy

As mentioned above floating point numbers are not able to represent all the
possible values over their bounds, but instead represent discrete values in that
interval, and approximate the remainder.

Additionally, floating point numbers are not evenly distributed over the real
number line, but instead are clustered closer together near zero, and further
apart as their magnitudes grow.

When discussing operations on floating point numbers, there is often reference
to a true value. This is the value that given no performance constraints and
infinite precision you would get, i.e `acos(1) = π`, where π has infinite
digits of precision.

For the CTS it is often sufficient to calculate the true value using TypeScript,
since its native number format is higher precision (double-precision/f64), and
all f32 values can be represented in it.

The true value is sometimes representable exactly as a f32 value, but often is
not.

Additionally, many operations are implemented using approximations from
numerical analysis, where there is a tradeoff between the precision of the
result and the cost.

Thus, the spec specifies what the accuracy constraints for specific operations
is, how close to truth an implementation is required to be, to be
considered conformant.

There are 5 different ways that accuracy requirements are defined in the spec:

1. *Exact*

   This is the situation where it is expected that true value for an operation
   is always expected to be exactly representable. This doesn't happen for any
   of the operations that return floating point values, but does occur for
   logical operations that return boolean values.


2. *Correctly Rounded*

   For the case that the true value is exactly representable as a f32, this is
   the equivalent of exactly from above. In the event that the true value is not
   exact, then the acceptable answer for most numbers is either the nearest f32
   above or the nearest f32 below the true value.

   For values near the subnormal range, e.g. close to zero, this becomes more
   complex, since an implementation may FTZ at any point. So if the exact
   solution is subnormal or either of the neighbours of the true value are
   subnormal, zero becomes a possible result, thus the acceptance interval is
   wider than naively expected.


3. *Absolute Error*

   This type of accuracy specifies an error value, ε, and the calculated result
   is expected to be within that distance from the true value, i.e.
   `[ X - ε, X + ε ]`.

   The main drawback with this manner of specifying accuracy is that it doesn't
   scale with the level of precision in floating point numbers themselves at a
   specific value. Thus, it tends to be only used for specifying accuracy over
   specific limited intervals, i.e. [-π, π].


4. *Units of Least Precision (ULP)*

   The solution to the issue of not scaling with precision of floating point is
   to use units of least precision.

   ULP(X) is min (b-a) over all pairs (a,b) of representable floating point
   numbers such that (a <= X <= b and a =/= b). For a more formal discussion of
   ULP see
   [On the definition of ulp(x)](https://hal.inria.fr/inria-00070503/document).

   n * ULP or nULP means `[X - n * ULP @ X, X + n * ULP @ X]`.


5. *Inherited*

   When an operation's accuracy is defined in terms of other operations, then
   its accuracy is said to be inherited. Handling of inherited accuracies is
   one of the main driving factors in the design of testing framework, so will
   need to be discussed in detail.

## Acceptance Intervals

The first four accuracy types; Exact, Correctly Rounded, Absolute Error, and
ULP, sometimes called simple accuracies, can be defined in isolation from each
other, and by association can be implemented using relatively independent
implementations.

The original implementation of the floating point framework did this as it was
being built out, but ran into difficulties when defining the inherited
accuracies.

For examples, `tan(x) inherits from sin(x)/cos(x)`, one can take the defined
rules and manually build up a bespoke solution for checking the results, but
this is tedious, error-prone, and doesn't allow for code re-use.

Instead, it would be better if there was a single conceptual framework that one
can express all the 'simple' accuracy requirements in, and then have a mechanism
for composing them to define inherited accuracies.

In the WebGPU CTS this is done via the concept of acceptance intervals, which is
derived from a similar concept in the Vulkan CTS, though implemented
significantly differently.

The core of this idea is that each of different accuracy types can be integrated
into the definition of the operation, so that instead of transforming an input
from the domain to a point in the range, the operation is producing an interval
in the range, that is the acceptable values an implementation may emit.


The simple accuracies can be defined as follows:

1. *Exact*

   `f(x) => [X, X]`


2. *Correctly Rounded*

   If `X` is precisely defined as a f32

   `f(x) => [X, X]`

   otherwise,

   `[a, b]` where `a` is the largest representable number with `a <= X`, and `b`
   is the smallest representable number with `X <= b`


3. *Absolute Error*

   `f(x) => [ X - ε, X + ε ]`, where ε is the absolute error value


4. **ULP Error**

   `f(x) = X => [X - n*ULP(X), X + n*ULP(X)]`

As defined, these definitions handle mapping from a point in the domain into an
interval in the range.

This is insufficient for implementing inherited accuracies, since inheritance
sometimes involve mapping domain intervals to range intervals.

Here we use the convention for naturally extending a function on real numbers
into a function on intervals of real numbers, i.e. `f([a, b]) = [A, B]`.

Given that floating point numbers have a finite number of precise values for any
given interval, one could implement just running the accuracy computation for
every point in the interval and then spanning together the resultant intervals.
That would be very inefficient though and make your reviewer sad to read.

For mapping intervals to intervals the key insight is that we only need to be
concerned with the extrema of the operation in the interval, since the
acceptance interval is the bounds of the possible outputs.

In more precise terms:
```
  f(x) => X, x = [a, b] and X = [A, B]

  X = [min(f(x)), max(f(x))]
  X = [min(f([a, b])), max(f([a, b]))]
  X = [f(m), f(M)]
```
where m and M are in `[a, b]`, `m <= M`, and produce the min and max results
for `f` on the interval, respectively.

So how do we find the minima and maxima for our operation in the domain?

The common general solution for this requires using calculus to calculate the
derivative of `f`, `f'`, and then find the zeroes `f'` to find inflection
points of `f`.

This solution wouldn't be sufficient for all builtins, i.e. `step` which is not
differentiable at 'edge' values.

Thankfully we do not need a general solution for the CTS, since all the builtin
operations are defined in the spec, so `f` is from a known set of options.

These operations can be divided into two broad categories: monotonic, and
non-monotonic, with respect to an interval.

The monotonic operations are ones that preserve the order of inputs in their
outputs (or reverse it). Their graph only ever decreases or increases,
never changing from one or the other, though it can have flat sections.

The non-monotonic operations are ones whose graph would have both regions of
increase and decrease.

The monotonic operations, when mapping an interval to an interval, are simple to
handle, since the extrema are guaranteed to be the ends of the domain, `a` and `b`.

So `f([a, b])` = `[f(a), f(b)]` or `[f(b), f(a)]`. We could figure out if `f` is
increasing or decreasing beforehand to determine if it should be `[f(a), f(b)]`
or `[f(b), f(a)]`.

It is simpler to just use min & max to have an implementation that is agnostic
to the details of `f`.
```
  A = f(a), B = f(b)
  X = [min(A, B), max(A, B)]
```

The non-monotonic functions that we need to handle for interval-to-interval
mappings are more complex. Thankfully are a small number of the overall
operations that need to be handled, since they are only the operations that are
used in an inherited accuracy and take in the output of another operation as
part of that inherited accuracy.

So in the CTS we just have bespoke implementations for each of them.

Part of the operation definition in the CTS is a function that takes in the
domain interval, and returns a sub-interval such that the subject function is
monotonic over that sub-interval, and hence the function's minima and maxima are
at the ends.

This adjusted domain interval can then be fed through the same machinery as the
monotonic functions.

### Inherited Accuracy

So with all of that background out of the way, we can now define an inherited
accuracy in terms of acceptance intervals.

The crux of this is the insight that the range of one operation can become the
domain of another operation to compose them together.

And since we have defined how to do this interval to interval mapping above,
transforming things becomes mechanical and thus implementable in reusable code.

When talking about inherited accuracies `f(x) => g(x)` is used to denote that
`f`'s accuracy is a defined as `g`.

An example to illustrate inherited accuracies:

```
  tan(x) => sin(x)/cos(x)

  sin(x) => [sin(x) - 2^-11, sin(x) + 2^-11]`
  cos(x) => [cos(x) - 2^-11, cos(x) + 2-11]

  x/y => [x/y - 2.5 * ULP(x/y), x/y + 2.5 * ULP(x/y)]
```

`sin(x)` and `cos(x)` are non-monotonic, so calculating out a closed generic
form over an interval is a pain, since the min and max vary depending on the
value of x. Let's isolate this to a single point, so you don't have to read
literally pages of expanded intervals.

```
  x = π/2

  sin(π/2) => [sin(π/2) - 2-11, sin(π/2) + 2-11]
           => [0 - 2-11, 0 + 2-11]
           => [-0.000488.., 0.000488...]
  cos(π/2) => [cos(π/2) - 2-11, cos(π/2) + 2-11]
           => [-0.500488, -0.499511...]

  tan(π/2) => sin(π/2)/cos(π/2)
           => [-0.000488.., 0.000488...]/[-0.500488..., -0.499511...]
           => [min({-0.000488.../-0.500488..., -0.000488.../-0.499511..., ...}),
               max(min({-0.000488.../-0.500488..., -0.000488.../-0.499511..., ...}) ]
           => [0.000488.../-0.499511..., 0.000488.../0.499511...]
           => [-0.0009775171, 0.0009775171]
```

For clarity this has omitted a bunch of complexity around FTZ behaviours, and
that these operations are only defined for specific domains, but the high-level
concepts hold.

For each of the inherited operations we could implement a manually written out
closed form solution, but that would be quite error-prone and not be
re-using code between builtins.

Instead, the CTS takes advantage of the fact in addition to testing
implementations of `tan(x)` we are going to be testing implementations of
`sin(x)`, `cos(x)` and `x/y`, so there should be functions to generate
acceptance intervals for those operations.

The `tan(x)` acceptance interval can be constructed by generating the acceptance
intervals for `sin(x)`, `cos(x)` and `x/y` via function calls and composing the
results.

This algorithmically looks something like this:

```
  tan(x):
    Calculate sin(x) interval
    Calculate cos(x) interval
    Calculate sin(x) result divided by cos(x) result
    Return division result
```

# Appendix

### Significant f32 Values

| Name                   |     Decimal (~) |         Hex | Sign Bit | Exponent Bits |             Significand Bits |
| ---------------------- | --------------: | ----------: | -------: | ------------: | ---------------------------: |
| Negative Infinity      |              -∞ | 0xff80 0000 |        1 |     1111 1111 | 0000 0000 0000 0000 0000 000 |
| Min Negative Normal    |  -3.40282346E38 | 0xff7f ffff |        1 |     1111 1110 | 1111 1111 1111 1111 1111 111 |
| Max Negative Normal    |  -1.1754943E−38 | 0x8080 0000 |        1 |     0000 0001 | 0000 0000 0000 0000 0000 000 |
| Min Negative Subnormal |  -1.1754942E-38 | 0x807f ffff |        1 |     0000 0000 | 1111 1111 1111 1111 1111 111 |
| Max Negative Subnormal |  -1.4012984E−45 | 0x8000 0001 |        1 |     0000 0000 | 0000 0000 0000 0000 0000 001 |
| Negative Zero          |              -0 | 0x8000 0000 |        1 |     0000 0000 | 0000 0000 0000 0000 0000 000 |
| Positive Zero          |               0 | 0x0000 0000 |        0 |     0000 0000 | 0000 0000 0000 0000 0000 000 |
| Min Positive Subnormal |   1.4012984E−45 | 0x0000 0001 |        0 |     0000 0000 | 0000 0000 0000 0000 0000 001 |
| Max Positive Subnormal |   1.1754942E-38 | 0x007f ffff |        0 |     0000 0000 | 1111 1111 1111 1111 1111 111 |
| Min Positive Normal    |   1.1754943E−38 | 0x0080 0000 |        0 |     0000 0001 | 0000 0000 0000 0000 0000 000 |
| Max Positive Normal    |   3.40282346E38 | 0x7f7f ffff |        0 |     1111 1110 | 1111 1111 1111 1111 1111 111 |
| Negative Infinity      |               ∞ | 0x7f80 0000 |        0 |     1111 1111 | 0000 0000 0000 0000 0000 000 |

# Resources
- [WebGPU Spec](https://www.w3.org/TR/webgpu/)
- [WGSL Spec](https://www.w3.org/TR/WGSL/)
- [float32 on Wikipedia](https://en.wikipedia.org/wiki/Single-precision_floating-point_format)
- [IEEE-754 Floating Point Converter](https://www.h-schmidt.net/FloatConverter/IEEE754.html)
- [IEEE 754 Calculator](http://weitz.de/ieee/)
- [Keisan High Precision Calculator](https://keisan.casio.com/calculator)
- [On the definition of ulp(x)](https://hal.inria.fr/inria-00070503/document)

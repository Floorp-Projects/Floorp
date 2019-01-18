# mapScopes

The files in this directory manage the Devtools logic for taking the original
JS file from the sourcemap and using the content, combined with source
mappings, to provide an enhanced user experience for debugging the code.

In this document, we'll refer to the files as either:

* `original` - The code that the user wrote originally.
* `generated` - The code actually executing in the engine, which may have been
  transformed from the `original`, and if a bundler was used, may contain the
  final output for several different `original` files.

The enhancements implemented here break down into as two primary improvements:

* Rendering the scopes as the user expects them, using the position in the
  original file, rather than the content of the generated file.
* Allowing users to interact with the code in the console as if it were the
  original code, rather than the generated file.


## Overall Approach

The core goal of scope mapping is to parse the original and generated files,
and then infer a correlation between the bindings in the original file,
and the bindings in the generated file. This is correlation is done via the
source mappings provided alongside the original code in the sourcemap.

The overall steps break down into:


### 1. Parsing

First the generated and original files are parsed into ASTs, and then the ASTs
are each traversed in order to generate a full picture of the scopes that are
present in the file. This covers information for each binding in each scope,
including all metadata about the declarations themselves, and all references
to each of the bindings. The exact location of each binding reference is the
most important factor because these will be used when working with sourcemaps.

Importantly, this scope tree's structure also mirrors the structure of the
scope data that the engine itself returns when paused.


### 2. Generated Binding -> Grip Correlation

When the engine pauses, we get back a full description of the current scope,
as well as `grip` objects that can be used as proxy-like objects in order to
access the actual values in the engine itself.

With this data, along with the location where the engine is paused, we can
use the AST-based scope information from step 1 to attach a line/column
range to each `grip`. Using those mappings we have enough information to get
the real engine value of any variable based on its position in the generated
code, as long as it is in scope at the paused location in the generated file.

The generated and engine scopes are correlated depth-first, because in some
cases the scope data from the engine will be deeper that the data from the
parsed code, meaning there are additional unknown parent scopes. This can
happen, for instance, if the executing code is running inside of an `eval`
since the parser only knows about the scopes inside of `eval`, and cannot
know what context it is executed inside of.


### 3. Original Binding -> Generated Binding Correlation

Now that we can get the value of a generated location, we need to decide which
generated location correlates with which original location. For each of
the original bindings in each original scope, we iterate through the
individual references to the binding in the file and:

1. Use the sourcemap to convert the original location into a range on the
   generated code.
2. Filter the available bindings in the generated file down to those that
   overlap with this generated range.
3. Perform heuristics to decide if any of the bindings in the range appear
   to be a valid and safe mapping.

These steps allow us to build a datastructure that describes the scope
as it is declared in the original file, while rendering the value using the
`grip` objects, as returned from the engine itself.

There is additional complexity here in the exact details of the heuristics
mentioned above. See the later Heuristics sections diving into these details.

During this phase, one additional task is performed, which is the construction
of expressions that relate back to the original binding. After matching each
binding in a range, we know the name of the binding in the original scope,
and we _also_ know the name of the generated binding, or more generally the
expression to evaluate in order to get the value of the original binding.

These expression values are important for ensuring that the developer console
is able to allow users to interact with the original code. If a user types
a variable into the console, it can be translated into the expression
correlated with that original variable, allowing the code to behave as it
would have, were it actually part of the original file.


### 4. Generate a Scope Tree

The structure generated in step 3 is converted into a structure compatible
with the standard scope data returned from the engine itself in order to allow
for consistent usage of scope data, irrespective of the scope data source.
This stage also re-attaches the global scope data, which is otherwise ignored
during the correlation process.


### 5. Validation

As a simple form of validation, we ensure that a large percentage of bindings
were actually resolved to a `grip` object. This offers some amount of
protection, if the sourcemap itself turned out to be somewhat low-quality.

This happens often with tooling that generates maps that simply map an original
line to a generated line. While line-to-line mappings still enable step
debugging, they do not provide enough information for original-scope mapping.

On the other hand, generally line-to-line mappings are usually generated by
tooling that performs minimal transformations on their own, so it is often
acceptable to fall back to the engine-only generated-file scope data in this
case.


## Range Mapping Heuristics

Since we know lots of information about the original bindings when performing
matches, it is possible to make educated guesses about how their generated
code will have likely been transformed. This allows us to perform more
aggressive matching logic what would normally be too false-positive-heavy
for generic use, when it is deemed safe enough.


### Standard Matching

In the general case, we iterate through the bindings that were found in the
generated range, and consider it a match if the binding overlaps with the
start of the range. One extra case here is that ranges at the start of
a line often point to the first binding in a range and do not overlap the
first binding, so there is special-casing for the first binding in each range.


### ES6 Import Reference Special Case

References to ES6 imports are often transformed into property accesses on objects
in order to emulate the "live-binding" behavior defined by ES6. The standard
logic here would end up always resolving the imported value to be the import
namespace object itself, rather than reading the value of the property.

To support this, specifically for ES6 imports, we walk outward from the matched
binding itself, taking property accesses into account, as long as the
property access itself is also within the mapped range.

While decently effective, there are currently two downsides to this approach:

* The "live" behavior of imports is often implemented using accessor
  properties, which as of the time of this writing, cannot be evaluated to
  retreive their real value.
* The "live" behavior of imports is sometimes implemented with function calls,
  which also also cannot be evaluated, causing their value to be
  unknown.


### ES6 Import Declaration Special Case

If there are no references to an imported value, or matching based on the
reference failed, we fall back to a second case.

ES6 import declarations themselves often map back to the location of the
declaration of the imported module's namespace object. By getting the range for
the import declaration itself, we can infer which generated binding is the
namespace object. Alongside that, we already know the name of the property on
the namespace itself because it is statically knowable by analyzing the
import declaration.

By combining both of those pieces of information, we can access the namespace's
property to get the imported value.


### Typescript Classes

Typescript have several non-ideal ways in which they output source maps, most
of which center around classes that are either exported, or decorated. These
issues are currently tracked in [Typescript issue #22833][1].

To work around this issue, we use a method similar to the import declaration
case above. While the class name itself often maps to unhelpful locations,
the class declaration itself generally maps to the class's transformed binding,
so we make use of the class declaration location instead of the location of
the class's declared name in these cases.

  [1]: https://github.com/Microsoft/TypeScript/issues/22833

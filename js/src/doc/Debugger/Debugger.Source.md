# Debugger.Source

A `Debugger.Source` instance represents either a piece of JavaScript source
code or the serialized text of a block of WebAssembly code. The two cases are
distinguished by the latter having its `introductionType` property always
being `"wasm"` and the former having its `introductionType` property never
being `"wasm"`.

Each [`Debugger`][debugger-object] instance has a separate collection of
`Debugger.Source` instances representing the source code that has been
presented to the system.

A debugger may place its own properties on `Debugger.Source` instances,
to store metadata about particular pieces of source code.

## Debugger.Source for JavaScript

For a `Debugger.Source` instance representing a piece of JavaScript source
code, its properties provide the source code itself as a string, and describe
where it came from. Each [`Debugger.Script`][script] instance refers to the
`Debugger.Source` instance holding the source code from which it was produced.

If a single piece of source code contains both top-level code and
function definitions, perhaps with nested functions, then the
[`Debugger.Script`][script] instances for those all refer to the same
`Debugger.Source` instance. Each script indicates the substring of the
overall source to which it corresponds.

A `Debugger.Source` instance may represent only a portion of a larger
source document. For example, an HTML document can contain JavaScript in
multiple `<script>` elements and event handler content attributes.
In this case, there may be either a single `Debugger.Source` instance
for the entire HTML document, with each [`Debugger.Script`][script] referring to
its substring of the document; or there may be a separate
`Debugger.Source` instance for each `<script>` element and
attribute. The choice is left up to the implementation.

If a given piece of source code is presented to the JavaScript
implementation more than once, with the same origin metadata, the
JavaScript implementation may generate a fresh `Debugger.Source`
instance to represent each presentation, or it may use a single
`Debugger.Source` instance to represent them all.

## Debugger.Source for WebAssembly

For a `Debugger.Source` instance representing the serialized text of a block
of WebAssembly code, its properties provide the serialized text as a string.

Currently only entire modules evaluated via `Wasm.instantiateModule` are
represented. SpiderMonkey constructs exactly one `Debugger.Source` for each
underlying WebAssembly module per [`Debugger`][debugger-object] instance.

Please note at the time of this writing, support for WebAssembly is very
preliminary. Many properties below return placeholder values.

## Convention

For descriptions of properties and methods below, if the behavior of the
property or method differs between the instance referring to JavaScript source
or to a block of WebAssembly code, the text will be split into two sections,
headed by "**if the instance refers to JavaScript source**" and "**if the
instance refers to WebAssembly code**", respectively. If the behavior does not
differ, no such emphasized headings will appear.

## Accessor Properties of the Debugger.Source Prototype Object

A `Debugger.Source` instance inherits the following accessor properties
from its prototype:

`canonicalId`
:   **If the instance refers to JavaScript source**, a stable, unique
    identifier for the source referent. This identifier is suitable for
    checking if two `Debugger.Source` instances originating from different
    `Debugger` instances refer to the same source that was compiled by
    SpiderMonkey. The `canonicalId` is reliable even when the source does not
    have a URL, or shares the same URL as another source but has different
    source text. It is more efficient to compare `canonicalId`s than to
    compare source text character-by-character. The `canonicalId` is not
    suitable for ordering comparisons such as "greater than" or "less
    than". It is not suitable for checking the equality of sources across
    worker threads.

    **If the instance refers to WebAssembly code**, throw a `TypeError`.

`text`
:   **If the instance refers to JavaScript source**, the JavaScript source
    code, as a string. The value satisfies the `Program`,
    `FunctionDeclaration`, or `FunctionExpression` productions in the
    ECMAScript standard.

    **If the instance refers to WebAssembly code**, the serialized text
    representation. The format is yet to be specified in the WebAssembly
    standard. Currently, the text is an s-expression based syntax.

`enclosingStart` <i>(future plan)</i>
:   The position within the enclosing document at which this source's text
    starts. This is a zero-based character offset. (The length of this
    script within the enclosing document is `source.length`.)

`lineCount` <i>(future plan)</i>
:   The number of lines in the source code. If there are characters after
    the last newline in the source code, those count as a final line;
    otherwise, `lineCount` is equal to the number of newlines in the source
    code.

`url`
:   **If the instance refers to JavaScript source**, the URL from which this
    source was loaded, if this source was loaded from a URL. Otherwise, this
    is `undefined`. Source may be loaded from a URL in the following ways:

    * The URL may appear as the `src` attribute of a `<script>` element
      in markup text.

    * The URL may be passed to the `Worker` web worker constructor, or the web
      worker `importScripts` function.

    * The URL may be the name of a XPCOM JavaScript module or subscript.

    (Note that code passed to `eval`, the `Function` constructor, or a
    similar function is <i>not</i> considered to be loaded from a URL; the
    `url` accessor on `Debugger.Source` instances for such sources should
    return `undefined`.)

    **If the instance refers to WebAssembly code**, the URL of the script that
    called `Wasm.instantiateModule` with the string `"> wasm"` appended.

`sourceMapURL`
:   **If the instance refers to JavaScript source**, if this source was
    produced by a minimizer or translated from some other language, and we
    know the URL of a <b>source map</b> document relating the source positions
    in this source to the corresponding source positions in the original
    source, then this property's value is that URL. Otherwise, this is `null`.

    (On the web, the translator may provide the source map URL in a
    specially formatted comment in the JavaScript source code, or via a
    header in the HTTP reply that carried the generated JavaScript.)

    This property is writable, so you can change the source map URL by
    setting it. All Debugger.Source objects referencing the same
    source will see the change. Setting an empty string has no affect
    and will not change existing value.

    **If the instance refers to WebAssembly code**, `null`. Attempts to write
    to this property throw a `TypeError`.

`element`
:   The [`Debugger.Object`][object] instance referring to the DOM element to which
    this source code belongs, if any, or `undefined` if it belongs to no DOM
    element. Source belongs to a DOM element in the following cases:

    * Source belongs to a `<script>` element if it is the element's text
      content (that is, it is written out as the body of the `<script>`
      element in the markup text), or is the source document referenced by its
      `src` attribute.

    * Source belongs to a DOM element if it is an event handler content
      attribute (that is, if it is written out in the markup text as an
      attribute value).

    * Source belongs to a DOM element if it was assigned to one of the
      element's event handler IDL attributes as a string. (Note that one may
      assign both strings and functions to DOM elements' event handler IDL
      attributes. If one assigns a function, that function's script's source
      does <i>not</i> belong to the DOM element; the function's definition
      must appear elsewhere.)

    (If the sources attached to a DOM element change, the `Debugger.Source`
    instances representing superceded code still refer to the DOM element;
    this accessor only reflects origins, not current relationships.)

`elementAttributeName`
:   If this source belongs to a DOM element because it is an event handler
    content attribute or an event handler IDL attribute, this is the name of
    that attribute, a string. Otherwise, this is `undefined`.

`introductionType`
:   **If the instance refers to JavaScript source**, a string indicating how
    this source code was introduced into the system.  This accessor returns
    one of the following values:

    * `"eval"`, for code passed to `eval`.

    * `"Function"`, for code passed to the `Function` constructor.

    * `"Worker"`, for code loaded by calling the Web worker constructor—the
      worker's main script.

    * `"importScripts"`, for code by calling `importScripts` in a web worker.

    * `"eventHandler"`, for code assigned to DOM elements' event handler IDL
      attributes as a string.

    * `"scriptElement"`, for code belonging to `<script>` elements.

    * `"javascriptURL"`, for code presented in `javascript:` URLs.

    * `"setTimeout"`, for code passed to `setTimeout` as a string.

    * `"setInterval"`, for code passed to `setInterval` as a string.

    * `undefined`, if the implementation doesn't know how the code was
      introduced.

    **If the instance refers to WebAssembly code**, `"wasm"`.

`introductionScript`, `introductionOffset`
:   **If the instance refers to JavaScript source**, and if this source was
    introduced by calling a function from debuggee code, then
    `introductionScript` is the [`Debugger.Script`][script] instance referring
    to the script containing that call, and `introductionOffset` is the call's
    bytecode offset within that script. Otherwise, these are both `undefined`.
    Taken together, these properties indicate the location of the introducing
    call.

    For the purposes of these accessors, assignments to accessor properties are
    treated as function calls. Thus, setting a DOM element's event handler IDL
    attribute by assigning to the corresponding JavaScript property creates a
    source whose `introductionScript` and `introductionOffset` refer to the
    property assignment.

    Since a `<script>` element parsed from a web page's original HTML was not
    introduced by any scripted call, its source's `introductionScript` and
    `introductionOffset` accessors both return `undefined`.

    If a `<script>` element was dynamically inserted into a document, then these
    accessors refer to the call that actually caused the script to run—usually
    the call that made the element part of the document. Thus, they do
    <i>not</i> refer to the call that created the element; stored the source as
    the element's text child; made the element a child of some uninserted parent
    node that was later inserted; or the like.

    Although the main script of a worker thread is introduced by a call to
    `Worker` or `SharedWorker`, these accessors always return `undefined` on
    such script's sources. A worker's main script source and the call that
    created the worker are always in separate threads, but
    [`Debugger`][debugger-object] is an inherently single-threaded facility: its
    debuggees must all run in the same thread. Since the global that created the
    worker is in a different thread, it is guaranteed not to be a debuggee of
    the [`Debugger`][debugger-object] instance that owns this source; and thus
    the creating call is never "in debuggee code". Relating a worker to its
    creator, and other multi-threaded debugging concerns, are out of scope for
    [`Debugger`][debugger-object].

    **If the instance refers to WebAssembly code**, `introductionScript` is
    the [`Debugger.Script`][script] instance referring to the same underlying
    WebAssembly module. `introductionOffset` is `undefined`.


## Function Properties of the Debugger.Source Prototype Object

<code>substring(<i>start</i>, [<i>end</i>])</code> <i>(future plan)</i>
:   Return a substring of this instance's `source` property, starting at
    <i>start</i> and extending to, but not including, <i>end</i>. If
    <i>end</i> is `undefined`, the substring returned extends to the end of
    the source.

    Both indices are zero-based. If either is `NaN` or negative, it is
    replaced with zero. If either is greater than the length of the source,
    it is replaced with the length of the source. If <i>start</i> is larger
    than <i>end</i>, they are swapped. (This is meant to be consistent with
    the way `String.prototype.substring` interprets its arguments.)

<code>lineToPosition(<i>line</i>)</code> <i>(future plan)</i>
:   Return an object of the form
    <code>{ line:<i>line</i>, start:<i>start</i>, length:<i>length</i> }</code>, where
    <i>start</i> is the character index within `source` of the first
    character of line number <i>line</i>, and <i>length</i> is the length of
    that line in characters, including the final newline, if any. The first
    line is numbered one. If <i>line</i> is negative or greater than the
    number of lines in this `Debugger.Source` instance, then return `null`.

<code>positionToLine(<i>start</i>)</code> <i>(future plan)</i>
:   Return an object of the form
    <code>{ line:<i>line</i>, start:<i>start</i>, length:<i>length</i> }</code>, where
    <i>line</i> is the line number containing the character position
    <i>start</i>, and <i>length</i> is the length of that line in
    characters, including the final newline, if any. The first line is
    numbered one. If <i>start</i> is negative or greater than the length of
    the source code, then return `null`.

<code>findScripts(<i>query</i>)</code> <i>(future plan)</i>
:   Return an array of [`Debugger.Script`][script] instances for all debuggee scripts
    matching <i>query</i> that are produced from this `Debugger.Source`
    instance. Aside from the restriction to scripts produced from this
    source, <i>query</i> is interpreted as for
    `Debugger.prototype.findScripts`.

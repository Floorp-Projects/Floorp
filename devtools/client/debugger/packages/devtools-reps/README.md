# DevTools Reps

![](http://g.recordit.co/IxhfRP8pNf.gif)

Reps is Firefox DevTools' remote object formatter. It stands for _representation_.

## Example

```js
const React = require("react");
let {
  Rep,
  Grip,
  MODE,
  ObjectInspector,
  ObjectInspectorUtils
} = require("devtools-reps");

function renderRep({ object, mode }) {
  return Rep({ object, defaultRep: Grip, mode });
}

ReactDOM.render(
  Rep({ object, defaultRep: Grip, mode }),
  document.createElement("div")
);
```

## What is what?

### `Rep`

`Rep` is the top-level component that is capable of formatting any type.

Supported types:

> RegExp, StyleSheet, Event, DateTime, TextNode, Attribute, Func, ArrayRep, Document, Window, ObjectWithText, ObjectWithURL, GripArray, GripMap, Grip, Undefined, Null, StringRep, Number, SymbolRep,

### `Grip`

`Grip` is a client representation of a remote JS object and is used as an input object for this rep component.

## Running the tests

Reps tests are written with jest, and you can run them locally by executing `yarn test`.

## History

The Reps project was ported to Github January 18th, 2017. You can view the history of a file after that date on [github][history] or by running this query:

```bash
git log --before "2017-1-17" devtools/client/shared/components/reps
```

They were first moved to the [devtools-reps][gh-devtools-reps] repository, then to the [devtools-core][gh-devtools-core] one, before being migrated to this repository.

[history]: https://github.com/mozilla/gecko-dev/commits/master/devtools/client/shared/components/reps
[gh-devtools-reps]:
https://github.com/firefox-devtools/reps/commits/master
[gh-devtools-core]:
https://github.com/firefox-devtools/devtools-core/commits/5ba3d6f6a44def9978a983edd6f2f89747dca2c7/packages/devtools-reps

## License

[MPL 2](./LICENSE)

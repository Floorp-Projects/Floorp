# Backward Compatibility

## Overview

When making changes to the DevTools, there are certain backward compatibility requirements that we should keep in mind.

In general, we should strive to maintain feature support for existing servers as we continue to make changes to the code base. At times, this can be difficult to achieve, however.

## Specific Guidelines

The important compatibility scenarios are:

1. Nightly desktop client **MUST** maintain existing compatibility back to release channel servers.

This is mainly to simplify cross-platform use cases, i.e. desktop Nightly with release Fennec.

2. Servers **MAY** use traits to state a feature is not supported yet.

This helps us support alternate environments, which does not implement every possible server feature.

Certainly when a new feature needs a new actor method to function, it won't work with servers that don't support it. But we should still ensure the client doesn't explode when using unrelated, existing features, at least until the above time windows have elapsed.

## Testing

The harder part of this currently is that there is no automated testing to ensure the above guidelines have been met. While we hope to have this at some point, for now manual testing is needed here.

The easiest way to test this is to check your work against a Firefox for Android device on release channel to ensure existing features in the area you are changing continue to function. That doesn't cover every case, but it's a great start.

## Feature Detection

Starting with Firefox 36 (thanks to [bug 1069673](https://bugzilla.mozilla.org/show_bug.cgi?id=1069673)), you can use actor feature detection to determine which actors exist and what methods they expose.

1. Detecting if the server has an actor: all you need is access to the `Toolbox` instance, which all panels do, when they get instantiated. Then you can do:

```js
let hasPerformanceActor = toolbox.target.hasActor("performance");
```

The `hasActor` method returns a boolean synchronously.

2. Detecting if an actor has a given method: same thing here, you need access to the toolbox:

```js
toolbox.target.actorHasMethod("domwalker", "duplicateNode").then(hasMethod => {

}).catch(console.error);
```

The `actorHasMethod` returns a promise that resolves to a boolean.

## Removing old backward compatibility code

We used to support old Firefox OS servers (back to Gecko 28), we don't anymore. Does this mean we can remove compatibility traits for it?

Maybe. Keep in mind that we still want to support Firefox for Android back to release channel, so we still want to keep traits in general. That's a much smaller time window than we supported for Firefox OS, so it should allow for some simplification.

The type of compatibility that could now be removed are things where the protocol semantics changed in some Gecko older than release channel and the trait is not useful for saying "I don't support feature X".

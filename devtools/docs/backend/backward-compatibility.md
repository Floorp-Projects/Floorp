# Backward Compatibility

## Overview

When making changes to the DevTools, there are certain backward compatibility requirements that we should keep in mind.

In general, we should strive to maintain feature support for existing servers as we continue to make changes to the code base. At times, this can be difficult to achieve, however.

## Specific Guidelines

The important compatibility scenarios are:

- Nightly desktop client **MUST** maintain existing compatibility back to release channel servers.

This is mainly to simplify cross-platform use cases, i.e. desktop Nightly with release Fennec.

- Servers **MAY** use traits to state a feature is not supported yet.

This helps us support alternate environments, which does not implement every possible server feature.

Certainly when a new feature needs a new actor method to function, it won't work with servers that don't support it. But we should still ensure the client doesn't explode when using unrelated, existing features, at least until the above time windows have elapsed.

## Testing

The harder part of this currently is that there is no automated testing to ensure the above guidelines have been met. While we hope to have this at some point, for now manual testing is needed here.

The easiest way to test this is to check your work against a Firefox for Android device on release channel to ensure existing features in the area you are changing continue to function. That doesn't cover every case, but it's a great start.

## Feature Detection

Starting with Firefox 36 (thanks to [bug 1069673](https://bugzilla.mozilla.org/show_bug.cgi?id=1069673)), you can use actor feature detection to determine which actors exist.

### Target hasActor helper

Detecting if the server has an actor: all you need is access to the `Toolbox` instance, which all panels do, when they get instantiated. Then you can do:

```js
let hasPerformanceActor = toolbox.target.hasActor("performance");
```

The `hasActor` method returns a boolean synchronously.

### Traits

Expose traits on an Actor in order to flag certain features as available or not. For instance if a new method "someMethod" is added to an Actor, expose a "supportsSomeMethod" flag in the traits object for the Actor, set to true. When debugging older servers, the flag will be missing and will default to false.

Traits need to be forwarded to the client, and stored or used by the corresponding Front. There is no unique way of exposing traits, but there are still a few typical patterns found in the codebase.

For Actors using a "form()" method, for which the Front is automatically created by protocol.js, the usual pattern is to add a "traits" property to the form, that contains all the traits for the actor. The Front can then read the traits in its corresponding "form()" method. Example:
- [NodeActor form method](https://searchfox.org/mozilla-central/rev/e75e8e5b980ef18f4596a783fbc8a36621de7d1e/devtools/server/actors/inspector/node.js#209)
- [NodeFront form method](https://searchfox.org/mozilla-central/rev/e75e8e5b980ef18f4596a783fbc8a36621de7d1e/devtools/client/fronts/node.js#145)

For other Actors, there are two options. First option is to define the trait on the Root actor. Those traits will be available both via TargetMixin::getTrait(), and on DevToolsClient.traits. The second option is to implement a "getTraits()" method on the Actor, which will return the traits for the Actor. Example:
- [CompatibilityActor getTraits method](https://searchfox.org/mozilla-central/rev/e75e8e5b980ef18f4596a783fbc8a36621de7d1e/devtools/shared/specs/compatibility.js#40)
- [CompatibilitySpec getTraits definition](https://searchfox.org/mozilla-central/rev/e75e8e5b980ef18f4596a783fbc8a36621de7d1e/devtools/shared/specs/compatibility.js#40-43)
- [CompatibilityFront getTraits method](https://searchfox.org/mozilla-central/rev/e75e8e5b980ef18f4596a783fbc8a36621de7d1e/devtools/client/fronts/compatibility.js#41-47)

Ironically, "getTraits" needs to be handled with backwards compatibility. But there is no way to check that "getTraits" is available on the server other than performing a try catch around the method. See the CompatibilityFront example.

Whenever traits are added, make sure to add a relevant backward compatibility comment so that we know when the trait can be removed.

## Removing old backward compatibility code

We used to support old Firefox OS servers (back to Gecko 28), we don't anymore. Does this mean we can remove compatibility traits for it?

Maybe. Keep in mind that we still want to support Firefox for Android back to release channel, so we still want to keep traits in general. That's a much smaller time window than we supported for Firefox OS, so it should allow for some simplification.

The type of compatibility that could now be removed are things where the protocol semantics changed in some Gecko older than release channel and the trait is not useful for saying "I don't support feature X".

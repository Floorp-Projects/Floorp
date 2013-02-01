<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Irakli Gozalishvili [gozala@mozilla.com] -->

The `content` module re-exports three objects from
three other modules: [`Loader`](modules/sdk/content/loader.html),
[`Worker`](modules/sdk/content/worker.html), and
[`Symbiont`](modules/sdk/content/symbiont.html).

These objects are used in the internal implementations of SDK modules which use
[content scripts to interact with web content](dev-guide/guides/content-scripts/index.html),
such as the [`panel`](modules/sdk/panel.html) or [`page-mod`](modules/sdk/page-mod.html)
modules.

[Loader]:
[Worker]:modules/sdk/content/worker.html
[Symbiont]:modules/sdk/content/symbiont.html


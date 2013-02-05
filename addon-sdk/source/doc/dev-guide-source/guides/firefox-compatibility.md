<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Firefox Compatibility #

One of the promises the SDK makes is to maintain compatibility for its
["supported" or "high-level" APIs](modules/high-level-modules.html):
meaning that code written against them will not need to change as new
versions of Firefox are released.

This ties the SDK release cycle into the Firefox release cycle because
the SDK must absorb any changes made to Firefox APIs. The SDK
and Firefox both release every 6 weeks, and the releases are precisely
staggered: so the SDK releases three weeks before Firefox. Each SDK
release is tested against, and marked as compatible with, two
versions of Firefox:

* the currently shipping Firefox version at the time the SDK is released
* the Beta Firefox version at the time the SDK is released - which,
because SDK and Firefox releases are staggered, will become the
currently shipping Firefox three week later

Add-ons built using a particular version of the SDK are marked
as being compatible with those two versions of Firefox, meaning
that in the
[`targetApplication` field of the add-on's install.rdf](https://developer.mozilla.org/en/Install.rdf#targetApplication):

* the `minVersion` is set to the currently shipping Firefox
* the `maxVersion` is set to the current Firefox Beta

See the
[SDK Release Schedule](https://wiki.mozilla.org/Jetpack/SDK_2012_Release_Schedule)
for the list of all SDK releases scheduled for 2012, along with the Firefox
versions they are compatible with.

## "Compatible By Default" ##

<span class="aside">There are exceptions to the "compatible by default" rule:
add-ons with binary XPCOM components, add-ons that have their compatibility
set to less than Firefox 4, and add-ons that are repeatedly reported as
incompatible, which are added to a compatibility override list.
</span>

From Firefox 10 onwards, Firefox treats add-ons as
"compatible by default": that is, even if the Firefox installing the
add-on is not inside the range defined in `targetApplication`,
Firefox will happily install it.

For example, although an add-on developed using version 1.6 of the SDK will be
marked as compatible with only versions 11 and 12 of Firefox, users of
Firefox 10 will still be able to install it.

But before version 10, Firefox assumed that add-ons were incompatible unless
they were marked as compatible in the `targetApplication` field: so an add-on
developed using SDK 1.6 will not install on Firefox 9.

## Changing minVersion and maxVersion Values ##

The `minVersion` and `maxVersion` values that are written into add-ons
generated with the SDK are taken from the template file found at:

<pre>
app-extension/install.rdf
</pre>

If you need to create add-ons which are compatible with a wider range of
Firefox releases, you can edit this file to change the
`minVersion...maxVersion` range.

Obviously, you should be careful to test the resulting add-on on the extra
versions of Firefox you are including, because the version of the SDK you
are using will not have been tested against them.

## Repacking Add-ons ##

Suppose you create an add-on using version 1.6 of the SDK, and upload it to
[https://addons.mozilla.org/](https://addons.mozilla.org/). It's compatible
with versions 11 and 12 of Firefox, and indicates that in its
`minVersion...maxVersion` range.

Now Firefox 13 is released. Suppose Firefox 13 does not change any of the
APIs used by version 1.6 of the SDK. In this case the add-on will still
work with Firefox 13.

But if Firefox 13 makes some incompatible changes, then the add-on will
no longer work. In this case, we'll notify developers, who will need to:

* download and install a new version of the SDK
* rebuild their add-on using this version of the SDK
* update their XPI on [https://addons.mozilla.org/](https://addons.mozilla.org/)
with the new version.

If you created the add-on using the [Add-on Builder](https://builder.addons.mozilla.org/)
rather than locally using the SDK, then it will be repacked automatically
and you don't have to do this.

### Future Plans ###

The reason add-ons need to be repacked when Firefox changes is that the
SDK modules used by an add-on are packaged as part of the add-on, rather
than part of Firefox. In the future we plan to start shipping SDK modules
in Firefox, and repacking will not be needed any more.

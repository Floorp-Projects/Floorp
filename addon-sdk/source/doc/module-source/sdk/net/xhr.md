<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Atul Varma [atul@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->

The `xhr` module provides access to `XMLHttpRequest` functionality, also known
as AJAX.

## Limitations ##

The `XMLHttpRequest` object is currently fairly limited, and does not
yet implement the `addEventListener()` or `removeEventListener()`
methods. It also doesn't yet implement the `upload` property.

Furthermore, the `XMLHttpRequest` object does not currently support
the `mozBackgroundRequest` property. All security UI, such as
username/password prompts, are automatically suppressed, so if
required authentication information isn't passed to the `open()`
method, the request will fail.

## Resource Use ##

Whenever this module is unloaded, all in-progress requests are immediately
aborted.

## Security Concerns ##

By default, the `XMLHttpRequest` object grants full access to any
protocol scheme, which means that it can be used to read from (but not
write to) the host system's entire filesystem. It also has unfettered
access to any local area networks, VPNs, and the internet.

### Threat Model ###

The `XMLHttpRequest` object can be used by an add-on to "phone
home" and transmit potentially sensitive user data to third
parties.

If access to the filesystem isn't prevented, it could easily be used
to access sensitive user data, though this may be inconsequential if
the client can't access the network.

If access to local area networks isn't prevented, malicious code could access
sensitive data.

If transmission of cookies isn't prevented, malicious code could access
sensitive data.

Attenuating access based on a regular expression may be ineffective if
it's easy to write a regular expression that *looks* safe but contains
a special character or two that makes it far less secure than it seems
at first glance.

### Possible Attenuations ###

<span class="aside">
We may also want to consider attenuating further based on domain name
and possibly even restricting the protocol to `https:` only, to reduce
risk.
</span>

Before being exposed to unprivileged code, this object needs
to be attenuated in such a way that, at the very least, it can't
access the user's filesystem. This can probably be done most securely
by white-listing the protocols that can be used in the URL passed to
the `open()` method, and limiting them to `http:`, `https:`, and
possibly a special scheme that can be used to access the add-on's
packaged, read-only resources.

Finally, we need to also consider attenuating http/https requests such
that they're "sandboxed" and don't communicate potentially sensitive
cookie information.

<api name="XMLHttpRequest">
@class

<api name="XMLHttpRequest">
@constructor
  Creates an `XMLHttpRequest`. This is a constructor, so its use should always
  be preceded by the `new` operator.  For more information about
  `XMLHttpRequest` objects, see the MDC page on
  [Using XMLHttpRequest](https://developer.mozilla.org/En/Using_XMLHttpRequest)
  and the Limitations section in this page.
</api>
</api>

<api name="getRequestCount">
@function
  Returns the number of `XMLHttpRequest` objects that are alive (i.e., currently
  active or about to be).
@returns {integer}
  The number of live `XMLHttpRequest` objects.
</api>

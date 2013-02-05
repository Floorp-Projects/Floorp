<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Atul Varma [atul@mozilla.com]  -->
<!-- contributed by Drew Willcoxon [adw@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->

The `unload` module allows modules to register callbacks that are called
when they are unloaded.

<api name="ensure">
@function
  Calling `ensure()` on an object does two things:

  1. It replaces a destructor method with a wrapper method that will never call
     the destructor more than once.
  2. It ensures that this wrapper method is called when the object's module is
  unloaded.

  Therefore, when you register an object with `ensure()`, you can call its
  destructor method yourself, you can let it happen for you, or you can do both.

  The destructor will be called with a single argument describing the reason
  for the unload; see `when()`. If `object` does not have the expected 
  destructor method, then an exception is thrown when `ensure()` is called.

@param object {object}
  An object that defines a destructor method.
@param [name] {string}
  Optional name of the destructor method. Default is `unload`.
</api>

<api name="when">
@function
  Registers a function to be called when the module is unloaded.

@param callback {function}
  A function that will be called when the module is unloaded. It is called
  with a single argument, one of the following strings describing the reason
  for unload: `"uninstall"`, `"disable"`, `"shutdown"`, `"upgrade"`, or
  `"downgrade"`. If a reason could not be determined, `undefined` will be
  passed instead.
  Note that if an add-on is unloaded with reason `"disable"`, it will not be
  notified about `"uninstall"` while it is disabled. See
  [bug 571049](https://bugzilla.mozilla.org/show_bug.cgi?id=571049).
</api>

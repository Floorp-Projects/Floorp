<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Drew Willcoxon [adw@mozilla.com]  -->

The `notifications` module allows you to display transient,
[toaster](http://en.wikipedia.org/wiki/Toast_%28computing%29)-style
desktop messages to the user.

This API supports desktop notifications on Windows, OS X using
[Growl](http://growl.info/), and Linux using libnotify.  If the user's system
does not support desktop notifications or if its notifications service is not
running, then notifications made with this API are logged to Firefox's error
console and, if the user launched Firefox from the command line, the terminal.

Examples
--------

Here's a typical example.  When the message is clicked, a string is logged to
the console.

    var notifications = require("sdk/notifications");
    notifications.notify({
      title: "Jabberwocky",
      text: "'Twas brillig, and the slithy toves",
      data: "did gyre and gimble in the wabe",
      onClick: function (data) {
        console.log(data);
        // console.log(this.data) would produce the same result.
      }
    });

This one displays an icon that's stored in the add-on's `data` directory.  (See
the [`self`](modules/sdk/self.html) module documentation for more information.)

    var notifications = require("sdk/notifications");
    var self = require("self");
    var myIconURL = self.data.url("myIcon.png");
    notifications.notify({
      text: "I have an icon!",
      iconURL: myIconURL
    });


<api name="notify">
@function
  Displays a transient notification to the user.
@param options {object}
  An object with the following keys.  Each is optional.
  @prop [title] {string}
    A string to display as the message's title.
  @prop [text] {string}
    A string to display as the body of the message.
  @prop [iconURL] {string}
    The URL of an icon to display inside the message.  It may be a remote URL,
    a data URI, or a URL returned by the [`self`](modules/sdk/self.html)
    module.
  @prop [onClick] {function}
    A function to be called when the user clicks the message.  It will be passed
    the value of `data`.
  @prop [data] {string}
    A string that will be passed to `onClick`.
</api>

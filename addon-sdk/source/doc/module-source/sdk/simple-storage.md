<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `simple-storage` module lets you easily and persistently store data across
Firefox restarts.  If you're familiar with [DOM storage][] on the Web, it's
kind of like that, but for add-ons.

[DOM storage]: https://developer.mozilla.org/en/DOM/Storage

The simple storage module exports an object called `storage` that is persistent
and private to your add-on.  It's a normal JavaScript object, and you can treat
it as you would any other.

To store a value, just assign it to a property on `storage`:

    var ss = require("sdk/simple-storage");
    ss.storage.myArray = [1, 1, 2, 3, 5, 8, 13];
    ss.storage.myBoolean = true;
    ss.storage.myNull = null;
    ss.storage.myNumber = 3.1337;
    ss.storage.myObject = { a: "foo", b: { c: true }, d: null };
    ss.storage.myString = "O frabjous day!";

You can store array, boolean, number, object, null, and string values.
If you'd like to store other types of values, you'll first have to convert
them to strings or another one of these types.

Be careful to set properties on the `storage` object and not the module itself:

    // This is no good!
    var ss = require("sdk/simple-storage");
    ss.foo = "I will not be saved! :(";

Simple Storage and "cfx run"
----------------------------
The simple storage module stores its data in your profile.
Because `cfx run` by default uses a fresh profile each time it runs,
simple storage won't work with add-ons executed using `cfx run` - that
is, stored data will not persist from one run to the next.

The easiest solution to this problem is to use the
[`--profiledir` option to `cfx run`](dev-guide/cfx-tool.html#profiledir).

If you use this method, you must end your debugging session by
quitting Firefox normally, not by cancelling the shell command.
If you don't close Firefox normally, then simple storage will
not be notified that the session is finished, and will not write
your data to the backing store.

Constructing Arrays
-------------------
Be careful to construct array objects conditionally in your code, or you may
zero them each time the construction code runs. For example, this add-on
tries to store the URLs of pages the user visits:

<pre><code>
var ss = require("sdk/simple-storage");
ss.storage.pages = [];

require("sdk/tabs").on("ready", function(tab) {
  ss.storage.pages.push(tab.url);
});

var widget = require("sdk/widget").Widget({
  id: "log_history",
  label: "Log History",
  width: 30,
  content: "Log",
  onClick: function() {
    console.log(ss.storage.pages);
  }
});
</code></pre>

But this isn't going to work, because it empties the array each time the
add-on runs (for example, each time Firefox is started). Line 2 needs
to be made conditional, so the array is only constructed if it does
not already exist:

<pre><code>
if (!ss.storage.pages)
  ss.storage.pages = [];
</code></pre>

Deleting Data
-------------
You can delete properties using the `delete` operator. Here's an add-on
that adds three widgets to write, read, and delete a value:

<pre><code>
var widgets = require("sdk/widget");
var ss = require("sdk/simple-storage");

var widget = widgets.Widget({
  id: "write",
  label: "Write",
  width: 50,
  content: "Write",
  onClick: function() {
    ss.storage.value = 1;
    console.log("Setting value");
  }
});

var widget = widgets.Widget({
  id: "read",
  label: "Read",
  width: 50,
  content: "Read",
  onClick: function() {
    console.log(ss.storage.value);
  }
});

var widget = widgets.Widget({
  id: "delete",
  label: "Delete",
  width: 50,
  content: "Delete",
  onClick: function() {
    delete ss.storage.value;
    console.log("Deleting value");
  }
});
</pre></code>

If you run it, you'll see that after clicking "Read" after clicking
"Delete" gives you the expected output:

<pre>
info: undefined
</pre>

Quotas
------
The simple storage available to your add-on is limited.  Currently this limit is
about five megabytes (5,242,880 bytes).  You can choose to be notified when you
go over quota, and you should respond by reducing the amount of data in storage.
If the user quits the application while you are over quota, all data stored
since the last time you were under quota will not be persisted.  You should not
let that happen.

To listen for quota notifications, register a listener for the `"OverQuota"`
event.  It will be called when your storage goes over quota.

    function myOnOverQuotaListener() {
      console.log("Uh oh.");
    }
    ss.on("OverQuota", myOnOverQuotaListener);

Listeners can also be removed:

    ss.removeListener("OverQuota", myOnOverQuotaListener);

To find out how much of your quota you're using, check the module's `quotaUsage`
property.  It indicates the percentage of quota your storage occupies.  If
you're within your quota, it's a number from 0 to 1, inclusive, and if you're
over, it's a number greater than 1.

Therefore, when you're notified that you're over quota, respond by removing
storage until your `quotaUsage` is less than or equal to 1.  Which particular
data you remove is up to you.  For example:

    ss.storage.myList = [ /* some long array */ ];
    ss.on("OverQuota", function () {
      while (ss.quotaUsage > 1)
        ss.storage.myList.pop();
    });


Private Browsing
----------------
If your storage is related to your users' Web history, personal information, or
other sensitive data, your add-on should respect
[private browsing](http://support.mozilla.com/en-US/kb/Private+Browsing).

To read about how to opt into private browsing mode and how to use the
SDK to avoid storing user data associated with private windows, refer to the
documentation for the
[`private-browsing` module](modules/sdk/private-browsing.html).

<api name="storage">
@property {object}
  A persistent object private to your add-on.  Properties with array, boolean,
  number, object, null, and string values will be persisted.
</api>

<api name="quotaUsage">
@property {number}
  A number in the range [0, Infinity) that indicates the percentage of quota
  occupied by storage.  A value in the range [0, 1] indicates that the storage
  is within quota.  A value greater than 1 indicates that the storage exceeds
  quota.
</api>

<api name="OverQuota">
@event
The module emits this event when your add-on's storage goes over its quota.
</api>

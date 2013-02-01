<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Title #

Some text here

<api name="test">
@function
This is a function which does nothing in particular.
@returns {object}
  @prop firststring {string} First string
  @prop firsturl {url} First URL
@param argOne {string} This is the first argument.
@param [argTwo] {bool} This is the second argument.
@param [argThree=default] {uri}
       This is the third and final argument. And this is
       a test of the ability to do multiple lines of
       text.
@param [options] Options Bag
  @prop [style] {string} Some style information.
  @prop [secondToLastOption=True] {bool} The last property.
  @prop [lastOption] {uri}
        And this time we have
        A multiline description
        Written as haiku
</api>

This text appears between the API blocks.

<api name="append">
@function
This is a list of options to specify modifications to your slideBar instance.
@param options
       Pass in all of your options here.
  @prop [icon] {uri} The HREF of an icon to show as the method of accessing your features slideBar
  @prop [html] {string/xml}
        The content of the feature, either as an HTML string,
        or an E4X document fragment.
  @prop [url] {uri} The url to load into the content area of the feature
  @prop [width] {int} Width of the content area and the selected slide size
  @prop [persist] {bool}
        Default slide behavior when being selected as follows:
        If true: blah; If false: double blah.
  @prop [autoReload] {bool} Automatically reload content on select
  @prop [onClick] {function} Callback when the icon is clicked
  @prop [onSelect] {function} Callback when the feature is selected
  @prop [onReady] {function} Callback when featured is loaded
</api>

Wooo, more text.

<api name="cool-func.dot">
@function
@returns {string} A value telling you just how cool you are.
A boa-constructor!
This description can go on for a while, and can even contain
some **realy** fancy things. Like `code`, or even
~~~~{.javascript}
// Some code!
~~~~
@param howMuch {string} How much cool it is.
@param [double=true] {bool}
       In case you just really need to double it.
@param [options] An object-bag of goodies.
  @prop callback {function} The callback
  @prop [random] {bool} Do something random?
@param [onemore] {bool} One more paramater
@param [options2]
       This is a full description of something
       that really sucks. Because I now have a multiline
       description of this thingy.
  @prop monkey {string} You heard me right
  @prop [freak=true] {bool}
        Yes, you are a freak.
</api>

<api name="random">
@function
A function that returns a random integer between 0 and 10.
@returns {int} The random number.
</api>

<api name="empty-class">
@class
This class contains nothing.
</api>

<api name="only-one-ctor">
@class
This class contains only one constructor.
<api name="one-constructor">
@constructor
@param [options] An object-bag of goodies.
</api>
</api>

<api name="two-ctors">
@class
This class contains two constructors.
<api name="one-constructor">
@constructor
The first constructor.
@param [options] An object-bag of goodies.
</api>
<api name="another-constructor">
@constructor
The second constructor.
@param [options] An object-bag of goodies.
</api>
</api>

<api name="ctor-and-method">
@class
This class contains one constructor and one method.
<api name="one-constructor">
@constructor
The first constructor.
@param [options] An object-bag of goodies.
</api>
<api name="a-method">
@method
Does things.
@param [options] An argument.
</api>
</api>

<api name="ctor-method-prop-event">
@class
This class contains one constructor, one method, one property and an event.
<api name="one-constructor">
@constructor
The first constructor.
@param [options] An object-bag of goodies.
</api>
<api name="a-method">
@method
Does things.
@param [options] An argument.
</api>
<api name="a-property">
@property {bool}
Represents stuff.
</api>
<api name="message">
@event
Event emitted when the content script sends a message to the add-on.
@argument {JSON}
The message itself as a JSON-serialized object.
</api>
</api>

<api name="open">
@event
A module-level event called open.
@argument {bool}
Yes, it's open.
</api>

Some more text here, at the end of the file.


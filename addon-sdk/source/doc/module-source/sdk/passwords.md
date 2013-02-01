<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Irakli Gozalishvili [gozala@mozilla.com]  -->

The `passwords` module allows add-ons to interact with Firefox's
[Password Manager](http://support.mozilla.com/en-US/kb/Remembering%20passwords)
to add, retrieve and remove stored credentials.

A _credential_ is the set of information a user supplies to authenticate
herself with a service. Typically a credential consists of a username and a
password.

Using this module you can:

1. **Search** for credentials which have been stored in the Password Manager.
   You can then use the credentials to access their related service (for
   example, by logging into a web site).

2. **Store** credentials in the Password Manager. You can store different sorts
   of credentials, as outlined in the "Credentials" section below.

3. **Remove** stored credentials from the Password Manager.

## Credentials ##

In this API, credentials are represented by objects.

You create credential objects to pass into the API, and the API also returns
credential objects to you.  The sections below explain both the properties you
should define on credential objects and the properties you can expect on
credential objects returned by the API.

All credential objects include `username` and `password` properties. Different
sorts of stored credentials include various additional properties, as
outlined in this section.

You can use the Passwords API with three sorts of credentials:

* Add-on credentials
* HTML form credentials
* HTTP Authentication credentials

### Add-on Credential ###

These are associated with your add-on rather than a particular web site.
They contain the following properties:

<table>
<colgroup>
<col width="25%">
</colgroup>
<tr>
  <td>
    <code>username</code>
  </td>
  <td>
    The username.
  </td>
</tr>

<tr>
  <td>
    <code>password</code>
  </td>
  <td>
    The password.
  </td>
</tr>

<tr>
  <td>
    <code>url</code>
  </td>
  <td>
    <p>For an add-on credential, this property is of the form:<br><code>
    addon:&lt;addon-id&gt;</code>, where <code>&lt;addon-id&gt;</code>
    is the add-on's
    <a href="dev-guide/guides/program-id.html">
    Program ID</a>.</p>
    <p>You don't supply this value when storing an add-on credential: it is
    automatically generated for you. However, you can use it to work out
    which stored credentials belong to your add-on by comparing it with the
    <code>uri</code> property of the
    <a href="modules/sdk/self.html"><code>self</code></a>
    module.</p>
  </td>
</tr>

<tr>
  <td>
    <code>realm</code>
  </td>
  <td>
    <p>You can use this as a name for the credential, to distinguish
    it from any other credentials you've stored.</p>
    <p>The realm is displayed
    in Firefox's Password Manager, under "Site", in brackets after the URL.
    For example, if the realm for a credential is "User Registration", then
    its "Site" field will look something like:</p>
    <code>addon:jid0-01mBBFyu0ZAXCFuB1JYKooSTKIc (User Registration)</code>
  </td>
</tr>

</table>

### HTML Form Credential ###

If a web service uses HTML forms to authenticate its users, then the
corresponding credential is an HTML Form credential.

It contains the following properties:

<table>
<colgroup>
<col width="25%">
</colgroup>
<tr>
  <td>
    <code>username</code>
  </td>
  <td>
    The username.
  </td>
</tr>

<tr>
  <td>
    <code>password</code>
  </td>
  <td>
    The password.
  </td>
</tr>

<tr>
  <td>
    <code>url</code>
  </td>
  <td>
    The URL for the web service which requires the credential.
    You should omit anything after the hostname and (optional) port.
  </td>
</tr>

<tr>
  <td>
    <code>formSubmitURL</code>
  </td>
  <td>
    The value of the form's "action" attribute.
    You should omit anything after the hostname and (optional) port.
    If the form doesn't contain an "action" attribute, this property should
    match the <code>url</code> property.
  </td>
</tr>

<tr>
  <td>
    <code>usernameField</code>
  </td>
  <td>
    The value of the "name" attribute for the form's username field.
  </td>
</tr>

<tr>
  <td>
    <code>passwordField</code>
  </td>
  <td>
    The value of the "name" attribute for the form's password field.
  </td>
</tr>

</table>

So: given a form at `http://www.example.com/login`
with the following HTML:

<script type="syntaxhighlighter" class="brush: html"><![CDATA[
<form action="http://login.example.com/foo/authenticate.cgi">
      <div>Please log in.</div>
      <label>Username:</label> <input type="text" name="uname">
      <label>Password:</label> <input type="password" name="pword">
</form>
]]>
</script>

The corresponding values for the credential (excluding username and password)
should be:

<pre>
  url: "http://www.example.com"
  formSubmitURL: "http://login.example.com"
  usernameField: "uname"
  passwordField: "pword"
</pre>

Note that for both `url` and `formSubmitURL`, the portion of the URL after the
hostname is omitted.

### HTTP Authentication Credential ###

These are used to authenticate the user to a web site
which uses HTTP Authentication, as detailed in
[RFC 2617](http://tools.ietf.org/html/rfc2617).
They contain the following properties:

<table>
<colgroup>
<col width="25%">
</colgroup>
<tr>
  <td>
    <code>username</code>
  </td>
  <td>
    The username.
  </td>
</tr>

<tr>
  <td>
    <code>password</code>
  </td>
  <td>
    The password.
  </td>
</tr>

<tr>
  <td>
    <code>url</code>
  </td>
  <td>
    The URL for the web service which requires the credential.
    You should omit anything after the hostname and (optional) port.
  </td>
</tr>

<tr>
  <td>
    <code>realm</code>
  </td>
  <td>
    <p>The WWW-Authenticate response header sent by the server may include a
    "realm" field as detailed in
    <a href="http://tools.ietf.org/html/rfc2617">RFC 2617</a>. If it does,
    this property contains the value for the "realm" field. Otherwise, it is
    omitted.</p>
    <p>The realm is displayed in Firefox's Password Manager, under "Site",
    in brackets after the URL.</p>
  </td>
</tr>

</table>

So: if a web server at `http://www.example.com` requested authentication with
a response code like this:

<pre>
  HTTP/1.0 401 Authorization Required
  Server: Apache/1.3.27
  WWW-Authenticate: Basic realm="ExampleCo Login"
</pre>

The corresponding values for the credential  (excluding username and password)
should be:

<pre>
  url: "http://www.example.com"
  realm: "ExampleCo Login"
</pre>

## onComplete and onError ##

This API is explicitly asynchronous, so all its functions take two callback
functions as additional options: `onComplete` and `onError`.

`onComplete` is called when the operation has completed successfully and
`onError` is called when the function encounters an error.

Because the `search` function is expected to return a list of matching
credentials, its `onComplete` option is mandatory. Because the other functions
don't return a value in case of success their `onComplete` options are
optional.

For all functions, `onError` is optional.

<api name="search">
@function

This function is used to retrieve a credential, or a list of credentials,
stored in the Password Manager.

You pass it any subset of the possible properties a credential can contain.
Credentials which match all the properties you supplied are returned as an
argument to the `onComplete` callback.

So if you pass in an empty set of properties, all stored credentials are
returned:

    function show_all_passwords() {
      require("sdk/passwords").search({
        onComplete: function onComplete(credentials) {
          credentials.forEach(function(credential) {
            console.log(credential.username);
            console.log(credential.password);
            });
          }
        });
      }

If you pass it a single property, only credentials matching that property are
returned:

    function show_passwords_for_joe() {
      require("sdk/passwords").search({
        username: "joe",
        onComplete: function onComplete(credentials) {
          credentials.forEach(function(credential) {
            console.log(credential.username);
            console.log(credential.password);
            });
          }
        });
      }

If you pass more than one property, returned credentials must match all of
them:

    function show_google_password_for_joe() {
      require("sdk/passwords").search({
        username: "joe",
        url: "https://www.google.com",
        onComplete: function onComplete(credentials) {
          credentials.forEach(function(credential) {
            console.log(credential.username);
            console.log(credential.password);
            });
          }
        });
      }

To retrieve only credentials associated with your add-on, use the `url`
property, initialized from `self.uri`:

    function show_my_addon_passwords() {
      require("sdk/passwords").search({
        url: require("sdk/self").uri,
        onComplete: function onComplete(credentials) {
          credentials.forEach(function(credential) {
            console.log(credential.username);
            console.log(credential.password);
            });
          }
        });
      }

@param options {object}
The `options` object may contain any credential properties. It is used to
restrict the set of credentials returned by the `search` function.

See "Credentials" above for details on what these properties should be.

Additionally, `options` must contain a function assigned to its `onComplete`
property: this is called when the function completes and is passed the set of
credentials retrieved.

`options` may contain a function assigned to its `onError` property, which is
called if the function encounters an error. `onError` is passed the error as an
[nsIException](https://developer.mozilla.org/en/nsIException) object.

@prop [username] {string}
The username for the credential.

@prop [password] {string}
The password for the credential.

@prop [url] {string}
The URL associated with the credential.

@prop [formSubmitURL] {string}
The URL an HTML form credential is submitted to.

@prop [realm] {string}
For HTTP Authentication credentials, the realm for which the credential was
requested. For add-on credentials, a name for the credential.

@prop [usernameField] {string}
The value of the `name` attribute for the user name input field in a form.

@prop [passwordField] {string}
The value of the `name` attribute for the password input field in a form.

@prop  onComplete {function}
The callback function that is called once the function completes successfully.
It is passed all the matching credentials as a list. This is the only
mandatory option.

@prop [onError] {function}
The callback function that is called if the function failed. The
callback is passed an `error` containing a reason of a failure: this is an
[nsIException](https://developer.mozilla.org/en/nsIException) object.

</api>

<api name="store">
@function

This function is used to store a credential in the Password Manager.

It takes an `options` object as an argument: this contains all the properties
for the new credential.

As different sorts of credentials contain different properties, the
appropriate options differ depending on the sort of credential being stored.

To store an add-on credential:

    require("sdk/passwords").store({
      realm: "User Registration",
      username: "joe",
      password: "SeCrEt123",
    });

To store an HTML form credential:

    require("sdk/passwords").store({
      url: "http://www.example.com",
      formSubmitURL: "http://login.example.com",
      username: "joe",
      usernameField: "uname",
      password: "SeCrEt123",
      passwordField: "pword"
    });

To store an HTTP Authentication credential:

    require("sdk/passwords").store({
      url: "http://www.example.com",
      realm: "ExampleCo Login",
      username: "joe",
      password: "SeCrEt123",
    });

See "Credentials" above for more details on how to set these properties.

The options parameter may also include `onComplete` and `onError`
callback functions, which are called when the function has completed
successfully and when it encounters an error, respectively. These options
are both optional.

@param options {object}
An object containing the properties of the credential to be stored, and
optional `onComplete` and `onError` callback functions.

@prop username {string}
The username for the credential.

@prop password {string}
The password for the credential.

@prop [url] {string}
The URL to which the credential applies. Omitted for add-on
credentials.

@prop [formSubmitURL] {string}
The URL a form-based credential was submitted to. Omitted for add-on
credentials and HTTP Authentication credentials.

@prop [realm] {string}
For HTTP Authentication credentials, the realm for which the credential was
requested. For add-on credentials, a name for the credential.

@prop [usernameField] {string}
The value of the `name` attribute for the username input in a form.
Omitted for add-on credentials and HTTP Authentication credentials.

@prop [passwordField] {string}
The value of the `name` attribute for the password input in a form.
Omitted for add-on credentials and HTTP Authentication credentials.

@prop  [onComplete] {function}
The callback function that is called once the function completes successfully.

@prop [onError] {function}
The callback function that is called if the function failed. The
callback is passed an `error` argument: this is an
[nsIException](https://developer.mozilla.org/en/nsIException) object.

</api>

<api name="remove">
@function

Removes a stored credential. You supply it all the properties of the credential
to remove, along with optional `onComplete` and `onError` callbacks.

Because you must supply all the credential's properties, it may be convenient
to call `search` first, and use its output as the input to `remove`. For
example, to remove all of joe's stored credentials:

    require("sdk/passwords").search({
      username: "joe",
      onComplete: function onComplete(credentials) {
        credentials.forEach(require("sdk/passwords").remove);
      })
    });

To change an existing credential just call `store` after `remove` succeeds:

    require("sdk/passwords").remove({
      realm: "User Registration",
      username: "joe",
      password: "SeCrEt123"
      onComplete: function onComplete() {
        require("sdk/passwords").store({
          realm: "User Registration",
          username: "joe",
          password: "{{new password}}"
        })
      }
    });

@param options {object}

An object containing all the properties of the credential to be removed,
and optional `onComplete` and `onError` callback functions.

@prop username {string}
The username for the credential.

@prop password {string}
The password for the credential.

@prop [url] {string}
The URL to which the credential applies. Omitted for add-on
credentials.

@prop [formSubmitURL] {string}
The URL a form-based credential was submitted to. Omitted for add-on
credentials and HTTP Authentication credentials.

@prop [realm] {string}
For HTTP Authentication credentials, the realm for which the credential was
requested. For add-on credentials, a name for the credential.

@prop [usernameField] {string}
The value of the `name` attribute for the username input in a form.
Omitted for add-on credentials and HTTP Authentication credentials.

@prop [passwordField] {string}
The value of the `name` attribute for the password input in a form.
Omitted for add-on credentials and HTTP Authentication credentials.

@prop  [onComplete] {function}
The callback function that is called once the function has completed
successfully.

@prop [onError] {function}
The callback function that is called if the function failed. The
callback is passed an `error` argument: this is an
[nsIException](https://developer.mozilla.org/en/nsIException) object.

</api>

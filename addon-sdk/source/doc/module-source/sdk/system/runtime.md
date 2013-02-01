<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Wes Kocher [kwierso@gmail.com]  -->

The `runtime` module provides access to information about Firefox's
runtime environment. All properties exposed are read-only.

For more information, see [nsIXULRuntime][nsIXULRuntime].
[nsIXULRuntime]: https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIXULRuntime

<api name="inSafeMode">
@property {boolean}
  This value is `true` if Firefox was started in safe mode,
  otherwise `false`.
</api>

<api name="OS">
@property {string}
  A string identifying the current operating system. For example, .
  `"WINNT"`, `"Darwin"`, or `"Linux"`. See [OS_TARGET][OS_TARGET]
  for a more complete list of possible values.

[OS_TARGET]: https://developer.mozilla.org/en/OS_TARGET
</api>

<api name="processType">
@property {long}
  The type of the caller's process, which will be one of these constants\:
<table>
  <tr>
    <th>Constant</th>
    <th>Value</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>PROCESS_TYPE_DEFAULT</td>
    <td>0</td>
    <td>The default (chrome) process.</td>
  </tr>

  <tr>
    <td>PROCESS_TYPE_PLUGIN</td>
    <td>1</td>
    <td>A plugin subprocess.</td>
  </tr>

  <tr>
    <td>PROCESS_TYPE_CONTENT</td>
    <td>2</td>
    <td>A content subprocess.</td>
  </tr>

  <tr>
    <td>PROCESS_TYPE_IPDLUNITTEST</td>
    <td>3</td>
    <td>An IPDL unit testing subprocess.</td>
  </tr>
</table>
</api>

<api name="widgetToolkit">
@property {string}
  A string identifying the target widget toolkit in use.
</api>

<api name="XPCOMABI">
@property {string}
  A string identifying the [ABI][ABI] of the current processor and compiler vtable.
  This string takes the form \<`processor`\>-\<`compilerABI`\>,
  for example\: "`x86-msvc`" or "`ppc-gcc3`".
[ABI]: https://developer.mozilla.org/en/XPCOM_ABI
</api>

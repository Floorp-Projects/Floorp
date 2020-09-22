Uploading symbols to Mozilla's symbol server
============================================

As a third-party releasing your own builds of Firefox or B2G, you should
consider uploading debug symbols from the builds to Mozilla's symbol
server. If you have not disabled crash reporting in your builds, crash
reports will be submitted to `Mozilla's crash reporting
server <https://crash-stats.mozilla.com/>`__. Without the debug symbols
that match your build the crash reports will not contain actionable
information.

Symbols can be uploaded either via a web browser or a web API.


Building a Symbol Package
-------------------------

To upload symbols, you need to build a symbol package. This is a
.tar.bz2 file which contains the symbol files in a specific directory
structure.

If you are building Firefox,or a similar application using the Mozilla
build system, you can build the symbol package using a make target:

::

   ./mach buildsymbols

This will create a symbol package in ``dist/`` named something like
``firefox-77.0a1.en-US.linux-x86_64.crashreporter-symbols.zip`` .


Uploading symbols
-----------------

Symbols are uploaded via your account on crash-stats.mozilla.org. Visit
https://crash-stats.mozilla.com/symbols/ and log in. Then request upload
permission by filing a bug in the Socorro component using `this
template <https://bugzilla.mozilla.org/enter_bug.cgi?assigned_to=nobody%40mozilla.org&bug_ignored=0&bug_severity=normal&bug_status=NEW&cc=benjamin%40smedbergs.us&cc=chris.lonnen%40gmail.com&cf_blocking_b2g=---&cf_fx_iteration=---&cf_fx_points=---&cf_status_b2g_1_4=---&cf_status_b2g_2_0=---&cf_status_b2g_2_0m=---&cf_status_b2g_2_1=---&cf_status_b2g_2_2=---&cf_status_firefox32=---&cf_status_firefox33=---&cf_status_firefox34=---&cf_status_firefox35=---&cf_status_firefox_esr31=---&cf_tracking_firefox32=---&cf_tracking_firefox33=---&cf_tracking_firefox34=---&cf_tracking_firefox35=---&cf_tracking_firefox_esr31=---&cf_tracking_firefox_relnote=---&cf_tracking_relnote_b2g=---&comment=What%20Persona%20account%20%28email%29%20are%20you%20requesting%20access%20for%3F%0D%0A...%0D%0A%0D%0AWhat%20symbols%20will%20you%20be%20uploading%20using%20this%20account%3F%0D%0A...%0D%0A%0D%0AIs%20there%20somebody%20at%20Mozilla%20who%20can%20vouch%20for%20you%3F%0D%0A...%0D%0A&component=Infra&contenttypemethod=autodetect&contenttypeselection=text%2Fplain&defined_groups=1&flag_type-37=X&flag_type-4=X&flag_type-607=X&flag_type-781=X&flag_type-787=X&flag_type-791=X&flag_type-800=X&flag_type-803=X&form_name=enter_bug&maketemplate=Remember%20values%20as%20bookmarkable%20template&op_sys=All&priority=--&product=Socorro&rep_platform=All&short_desc=Symbol-upload%20permission%20for%20%3CPerson%3E&target_milestone=---&version=unspecified&format=__default__>`__.

After symbol upload is turned on, you can upload the symbol archive
directly using the web form at
https://crash-stats.mozilla.com/symbols/upload/web/. It is also possible
to upload via automated scripts: see the `symbol upload API
docs <https://crash-stats.mozilla.com/symbols/upload/api/>`__ for more
details.

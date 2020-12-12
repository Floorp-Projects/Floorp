Uploading symbols to Mozilla's symbol server
============================================

As a third-party releasing your own builds of Firefox or B2G, you should
consider uploading debug symbols from the builds to Mozilla's symbol
server. If you have not disabled crash reporting in your builds, crash
reports will be submitted to `Mozilla's crash reporting
server <https://crash-stats.mozilla.org/>`__. Without the debug symbols
that match your build the crash reports will not contain actionable
information.

Symbols can be uploaded either via a web browser or a web API.


Building a Symbol Package
-------------------------

To upload symbols, you need to build a symbol package. This is a
.zip file which contains the symbol files in a specific directory structure.

If you are building Firefox,or a similar application using the Mozilla
build system, you can build the symbol package using a make target:

::

   ./mach buildsymbols

This will create a symbol package in ``dist/`` named something like
``firefox-77.0a1.en-US.linux-x86_64.crashreporter-symbols.zip`` .

This step requires the ``dump_syms`` tool which should have been automatically
installed when you setup the Firefox build with ``./mach bootstrap``. If for
some reason it's missing or outdated running the bootstrap step again will
retrieve and install an up-to-date version of the tool.

Uploading symbols
-----------------

Symbols are uploaded via your account on symbols.mozilla.org. Visit
https://symbols.mozilla.org and log in. Then request upload
permission by filing a bug in the Socorro component using `this
template <https://bugzilla.mozilla.org/enter_bug.cgi?assigned_to=nobody%40mozilla.org&amp;bug_ignored=0&amp;bug_severity=--&amp;bug_status=NEW&amp;bug_type=task&amp;cc=gsvelto%40mozilla.com&amp;cc=willkg%40mozilla.com&amp;cf_fx_iteration=---&amp;cf_fx_points=---&amp;comment=What%20e-mail%20account%20are%20you%20requesting%20access%20for%3F%0D%0A...%0D%0A%0D%0AWhat%20symbols%20will%20you%20be%20uploading%20using%20this%20account%3F%0D%0A...%0D%0A%0D%0AIs%20there%20somebody%20at%20Mozilla%20who%20can%20vouch%20for%20you%3F%0D%0A...%0D%0A&amp;component=Upload&amp;contenttypemethod=list&amp;contenttypeselection=text%2Fplain&amp;defined_groups=1&amp;filed_via=standard_form&amp;flag_type-4=X&amp;flag_type-607=X&amp;flag_type-800=X&amp;flag_type-803=X&amp;flag_type-936=X&amp;form_name=enter_bug&amp;maketemplate=Remember%20values%20as%20bookmarkable%20template&amp;op_sys=Unspecified&amp;priority=--&amp;product=Tecken&amp;rep_platform=Unspecified&amp;short_desc=Symbol-upload%20permission%20for%20%3CPerson%3E&amp;target_milestone=---&amp;version=unspecified>`__.
If you don't have an account yet use the template to request one.

After symbol upload is turned on, you can upload the symbol archive
directly using the web form at https://symbols.mozilla.org/uploads.
It is also possible to upload via automated scripts: see the `symbol upload API
docs <https://tecken.readthedocs.io/en/latest/>`__ for more
details.

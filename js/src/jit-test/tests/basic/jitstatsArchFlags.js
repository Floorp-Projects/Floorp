// Make sure the arch flags are valid on startup, even if nothing has
// been traced yet. We don't know what arch the user is building on,
// but presumably we want at least 1 flag to be set on all supported
// platforms.

if (HAVE_TM) {
  assertEq(jitstats.archIsIA32 ||
	   jitstats.archIs64BIT ||
	   jitstats.archIsARM ||
	   jitstats.archIsSPARC ||
	   jitstats.archIsPPC ||
	   jitstats.archIsAMD64,
	   1);
 }

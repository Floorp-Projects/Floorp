This folder contains project files for CodeWarrior Pro 4.
The folder "(old projects)" contains older project files for
THINK C, CodeWarrior 5, 7, and 11. The older projects are
provided for reference only; only the latest project files
for CodeWarrior Pro 4 (gc.mcp, gctest.mcp and gctest++.mcp)
are supported.

To compile the garbage collector as a static library, open
"gc.mcp" with CodeWarrior IDE 3.3 (or later). Choose
"gctest.both" in the "Set Current Target" hierarchical
menu in the "Project" menu. Finally, choose "Make" from the
"Project" menu. This will build static libraries for both
the PowerPC and 68K-classic runtime architectures

To build the garbage collector test program, gctest, open
"gctest.mcp". Choose "gc.both" in the "Set Current Target"
hierarchical menu in the "Project" menu. Finally,
choose "Make" from the "Project" menu. This will build test
programs for both the PowerPC and 68K-classic runtime
architectures.

Patrick Beard
22 August, 1999

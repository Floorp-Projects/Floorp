#!perl

	use Moz;
	use BuildList;		# will be BuildList when we switch to the new world

$DEBUG = 1;
$MOZ_LITE = 0;		# build moz medium. This will come from a config file at some stage.
Moz::OpenErrorLog("::::Mozilla.BuildLog");

chdir(":::");

BuildMozilla();

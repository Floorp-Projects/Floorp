#!perl

	use Moz;
	use BuildList;

$DEBUG = 1;
$MOZ_LITE = 0;		# build moz medium. This will come from a config file at some stage.
Moz::OpenErrorLog("::::Mozilla.BuildLog");

BuildMozilla();

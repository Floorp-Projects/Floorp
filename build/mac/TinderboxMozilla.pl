#!perl

	use Moz;
	use BuildList;

$DEBUG = 1;
Moz::OpenErrorLog("::::Mozilla.BuildLog");

BuildMozilla();

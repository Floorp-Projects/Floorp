#!/usr/bin/perl

print "var sqlTables = {\n";

$in_item = 0;

while (<>) {
  chop;
  if (/--(.*)/) {
    print "    /* $1 */\n";
  }
  s/--.*$//;

  if (/^\s*CREATE TABLE (\S+)/) {
    print "  $1:\n";
    $in_item = 1;
  } elsif(/^\);/) {
    print "    \"\",\n\n";
    $in_item = 0;
  } else {
    if ($in_item) {
      next if /^\s*$/;
      print "    \"$_\" +\n";
    }
  }
}
print "};\n"

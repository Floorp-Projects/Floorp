#!C:/usr/local/bin/perl

# Usage: reanme perlexpr [files]

foreach $filename (@ARGV)
{
    $indexOfPeriod = 0;
    $i = 0;
    $temp = $filename;
    while ($i != -1)
    {
	$indexOfPeriod = $i;
	$i = index($temp, ".");
	$temp = substr($temp, 1+index($temp, "."));
    }

    $prefix = substr($filename, 0, $indexOfPeriod);
    $extension = substr($filename, 1 + $indexOfPeriod);

    $newName = "$prefix.hout";
    
    rename($filename, $newName);
}

#($op = shift) || die "Usage: rename perlexpr [filenames]\n";
#if (!@ARGV) {
#    @ARGV = <STDIN>;
#    chop(@ARGV);
#}
#for (@ARGV) {
#    $was = $_;
#    eval $op;
#    die $@ if $@;
#    rename($was,$_) unless $was eq $_;
#}

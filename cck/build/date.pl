($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime(time);
print "time... $sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst \n";
#$days = $yday + 1;
$mon = $mon + 1;
 
$len = length($mon);
if ($len < 2)  {
$mon = 0 . $mon
}

$len = length($mday);
if ($len < 2)  {
$mday = 0 . $mday
}

$year = $year + 1900;

$Blddate = $year . $mon . $mday . $hour;
open (BDATE, ">c:\\CCKScripts\\bdate.bat") || die "cannot open c:\\CCKScripts\\bdate.bat: $!";
print BDATE "set BuildID=$Blddate\n";

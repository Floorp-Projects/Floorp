#   4/7/99  Frank Petitta
#   1999 Netscape Communications Corp.
#   All rights reserved, must be over 18 to play.
# 
#  What is it?
#    Build, deliver the CCK parts and pieces.
#

printf("Begin CCK Setup.\n");

$BuildType = "";
$GoodBuild = 1;
$ErrorType = 0;
$SourceRoot = "";
$ContinousBuild = 0;


#  Use the ContinousBuild Var for Tinderboxen
#  I will also set the mailing to tinderbox, based off the value of 
#  ContinousBuild Var.
#while (ContinousBuild = 0){

#  Must have VC+ 6.0 or it's a no go.
if ($ENV{'_MSC_VER'}!=1200) {
	# go to some subroutine that will handle errors
	$ErrorType = 1;
	CFHandler($ErrorType);
}

# Lets see what the Source path is.
$SourceRoot = $ENV{'MOZ_SRC'};
$len = length($SourceRoot);
if ($len < 2)  {
	# Can't start if you dont know the Src Root.
	$ErrorType = 2;
	CFHandler($ErrorType);
}

#  Make sure MOZ_DEBUG is either 1 or 0
if ($ENV{'MOZ_DEBUG'} > 1 or $ENV{'MOZ_DEBUG'} < 0)  {
	$ErrorType = 3;
	CFHandler($ErrorType);
}

#  Now that we know MOZ_DEBUG is a 1 or 0, lets do something with it.
if ($ENV{'MOZ_DEBUG'}==0 && $ErrorType < 1) {
	$BuildType = "release";
}
elsif ($ENV{'MOZ_DEBUG'}==1 && $ErrorType < 1) {
	$BuildType = "debug";
}

# Email notification.
# I tried to use this file open/write method but,
# I kept getting "error reading tempfile.txt, aborting"
# So until I figuer it out I must use the .bat method......
#open (SENDFILE, ">c:\\CCKScripts\\tempfile.txt") || die "cannot open c:\\CCKScripts\\tempfile.txt: $!";
#print SENDFILE "CCK Build Starting\n";

#system("echo.CCK Build Starting. >> tempfile.txt");
#system("blat tempfile.txt -t page-petitta\@netscape.com -s \"CCK Build Notification\" -i Undertaker");
#system("if exist tempfile.txt del tempfile.txt");

printf("Begin CCK pull-build.\n");

# Get the Source Drive letter. And the Source Path.
@pieces = split(/\\/, $SourceRoot);
$SourceDrive = ("$pieces[0]");
@pieces = split(/:/, $SourceRoot);
$SourcePath = ("$pieces[$#pieces]");

# Now change the path to the build source.
chdir ("$SourceDrive");
chdir ("$SourcePath");
# Remove the old source, pull the new.
system ("echo y | rd /s mozilla");
system ("cvs co mozilla/cck");

# Lets build it
$TestPath = $SourcePath."\\mozilla\\cck\\driver";
chdir ($TestPath);

#  Gonna need a batch file to build.  This is because
#  of the fact that the PERL system command opens a new
#  session, thereby making the vcvars32.bat delaration 
#  invalid(different session)
#  
system ("call C:\\CCKScripts\\PERLBuild.bat");

if ($ENV{'BuildGood'}==1) {
	print ("Your mama");
}


print "$BuildType \n";
print "$SourceRoot \n";
print "$ErrorType \n";
print "$SourceDrive \n";
print "$SourcePath \n";
print "$TestPath \n";


#SetBuildDate();



#}

#  Compute and format the date string for the folder and build label.
sub SetBuildDate
{

($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime(time);
#print "time... $sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst \n";
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

$len = length($hour);
if ($len < 2)  {
$hour = 0 . $hour
}

$year = $year + 1900;

$Blddate = $year . "-" . $mon . "-" . $mday . "-" . $hour;
#open (BDATE, ">c:\\CCKScripts\\bdate.bat") || die "cannot open c:\\CCKScripts\\bdate.bat: $!");
#print BDATE "set BuildID=$Blddate\n";

printf($Blddate);

}


#  Handles all the errors ((CharlieFoxtrotHandler) Charlie = cluster, Foxtrot = f$*k)
sub CFHandler
{

if ($ErrorType==1)
{
	printf("Wrong ver. of Visual C+, must have Ver. 6.0 "|| die);
}

if ($ErrorType==2)
{
	printf("Cannot get the path to the Source base "|| die);
}

if ($ErrorType==3)
{
	printf("MOZ_DEBUG is not defined "|| die);	
}

if ($ErrorType==4)
{

}

if ($ErrorType==5)
{

}

if ($ErrorType==6)
{

}

if ($ErrorType==7)
{

}



#  END THIS THING!!!
quit;
die;
}
#! /tools/ns/bin/perl5.004


# A simple VCAL to ICAL converter
# There are a few variables you can tweek.
#
# Aug 8, 1998
# sman@netscape.com
#
# Aug 18 1998
# jsun@netscape.com
# Converts = continue lines to folded lines
# Converts =OD=OA to \\n
# Also, had to set DTSTAMP to fixed value for Windows
#
# The DTSTAMP value is going to default to *now*
# if you want to change it, this is the place...
#
$uname = `uname -a`;
if ($uname =~ /Windows/)
{
    $DTStamp = "DTSTAMP:19980818T112233Z\n";
}
else
{
    $DTStamp = "DTSTAMP:" . `date +19%y%m%dT%H%M%SZ`;
}


$ENV{'TZ'} = "GMT0";
$ENV{'TZ'} = "PST8PDT";

#
# The attendees are assumed to be in first name - last name format.
# Here at netscape, we can turn this into an e-mail address by simply
# replacing the spaces with underscores and adding "@netscape.com".
# The code below assumes the naming trick. However you can change
# the mailing domain by just setting the value below.
#
$domain = "\@netscape.com";

$foldedLine = 0;
while (<>)
{
    #print STDERR "foldedLine = $foldedLine\n"; 

    s/=0D=0A/\\n/;
    if ($foldedLine == 1)
    {
	# insert space to fold lines
	s/(.*)/ \1/;
	#print $_;
    }
    
    if (/=$/)
    {
	$foldedLine = 1;
	s/(.*)=$/\1/;
    }
    else
    {
	$foldedLine = 0;
    }
    
    if (/^VERSION:1.0$/)
    {
	print "VERSION:2.0\n";
    }
    elsif (/^PRODID:/)
    {
	print "PRODID:-//Netscape/Julian/whizbang-VCAL2ICAL/EN\n";
    }
    elsif (/^DTSTART:/)
    {
	print $DTStamp;
	print $_;
    }
    elsif (/^ATTENDEE/)
    {
	if (/ROLE=OWNER/)
	{
	    $temp = $_;
	    s/ROLE=OWNER/ROLE=REQ-PARTICIPANT/;
	    $temp =~ s/ATTENDEE/ORGANIZER/;
	    $temp =~ s/;ROLE=OWNER//;
	    $temp =~ s/;STATUS=CONFIRMED//;
	    $temp =~ s/:([^ ]+) (.*)/:mailto:\1_\2$domain/;
	    print $temp;
	}
	s/ROLE=ATTENDEE/ROLE=REQ-PARTICIPANT/ ;
	s/ROLE=OWNER/ROLE=REQ-PARTICIPANT/ ;
	s/NEEDS ACTION/NEEDS-ACTION/ ;
	s/STATUS/PARTSTAT/ ;
	s/CONFIRMED/ACCEPTED/ ;
	s/:([^ ]+) (.*)/:mailto:\1_\2$domain/;
	print $_;
    }
    else
    {
	print $_;
    }

}













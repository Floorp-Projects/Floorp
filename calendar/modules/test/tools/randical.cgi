#! /tools/ns/bin/perl5.004


# The contents of this file are subject to the Netscape Public License 
# Version 1.0 (the "NPL"); you may not use this file except in 
# compliance with the NPL.  You may obtain a copy of the NPL at 
# http://www.mozilla.org/NPL/ 
# 
# Software distributed under the NPL is distributed on an "AS IS" basis, 
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
# for the specific language governing rights and limitations under the 
# NPL. 
# 
# The Initial Developer of this code under the NPL is Netscape 
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
# Reserved. 
  
###################################################################

# file: rical.pl

# A random iCalendar file generator for testing the icalendar parser.
# Set the DATE_CMD environment variable if you are on Windows or Mac
# to point to the unix 'date' equivalent program.

# I have arbitrarily defined 3 categories of randomness that determine
# whether or not a property or parameter appears in the output. These
# are:
#       $MAY_prop_chances          # defaults to 5%
#       $SHOULD_prop_chances       # defaults to 65%
#       $MUST_prop_chances         # defaults to 95%

# You can vary these chances with command line options.

# sman
# 29-May-1998

#############################################################
#  Arrays for populating properties and parameters
#############################################################
@categories =  ('SPORTS','THEATER','CONVENTION','MEAL','VACATION',
	           'BIRTHDAY');
@class  =      ("PUBLIC", "PRIVATE", "CONFIDENTIAL", "X-VALUE");
@cnFirst =     ("Steve", "Joe", "Eric", "John", "Greg", "Terry",
		"Dave", "Scott", "Kevin", "Poindexter" );
@cnLast =      ("Smith", "Jones", "Washington", "DuBois", "Wells",
		"Vaughn", "Flintstone", "Simpson", "Smithers", "Gumble");
@cutypeparm =  ("INDIVIDUAL", "GROUP", "RESOURCE", "ROOM", "UNKNOWN", 
                   "X-TOKEN" );
@fbtypeparm =  ("FREE","BUSY","BUSY-UNAVAILABLE", "BUSY-TENTATIVE");
@method =      ("PUBLISH", "REQUEST", "REFRESH", "CANCEL", "ADD", "REPLY",
	           "COUNTER", "DECLINECOUNTER" );
@partstatparm =("NEEDS-ACTION", "ACCEPTED", "DECLINED", "TENTATIVE",
                   "DELEGATED", "x-name" );
@resources =   ("CAR", "TRUCK", "THEATER", "COUNTRY", "MILITARY",
	           "PROJECTOR", "STEREO", "CDPLAYER" );
@roleparm =    ("CHAIR", "REQ-PARTICIPANT", "OPT-PARTICIPANT", 
                   "NON-PARTICIPANT", "X-NAME" );
@status =      ("TENTATIVE", "CONFIRMED", "CANCELLED" );
@transparm =   ("OPAQUE","TRANSPARENT");

@rrules = (
    "FREQ=DAILY;COUNT=10",
    "FREQ=DAILY;UNTIL=19981224T000000Z",
    "FREQ=DAILY;INTERVAL=2",
    "FREQ=DAILY;INTERVAL=10;COUNT=5",
    "FREQ=DAILY;UNTIL=20000131T090000Z;BYMONTH=1",
    "FREQ=YEARLY;UNTIL=20000131T090000Z;BYMONTH=1;BYDAY=SU,MO,TU,WE,TH,FR,SA",
    "FREQ=WEEKLY;COUNT=10",
    "FREQ=WEEKLY;UNTIL=19981224T000000Z",
    "FREQ=WEEKLY;INTERVAL=2;WKST=SU",
    "FREQ=WEEKLY;COUNT=10;WKST=SU;BYDAY=TU,TH",
    "FREQ=WEEKLY;COUNT=10;INTERVAL=2;WKST=SU;BYDAY=TU,TH",
    "FREQ=MONTHLY;COUNT=10;INTERVAL=2;BYDAY=1FR",
    "FREQ=MONTHLY;COUNT=10;INTERVAL=2;BYDAY=1SU,-1SU",
    "FREQ=MONTHLY;COUNT=6;BYDAY=-2MO",
    "FREQ=MONTHLY;BYMONTHDAY=-3",
    "FREQ=MONTHLY;BYMONTHDAY=2,15",
    "FREQ=MONTHLY;COUNT=10;BYMONTHDAY=1,-1",
    "FREQ=MONTHLY;INTERVAL=18;COUNT=10;BYMONTHDAY=10,11,12,13,14,15",
    "FREQ=MONTHLY;INTERVAL=2;BYDAY=TU",
    "FREQ=YEARLY;COUNT=10;BYMONTH=6,7",
    "FREQ=YEARLY;INTERVAL=2;COUNT=10;BYMONTH=1,2,3",
    "FREQ=YEARLY;INTERVAL=3;COUNT=10;BYYEARDAY=1,100,200",
    "FREQ=YEARLY;BYDAY=20MO",
    "FREQ=YEARLY;BYWEEKNO=20;BYDAY=MO",
    "FREQ=YEARLY;BYMONTH=3;BYDAY=TH",
    "FREQ=YEARLY;BYDAY=TH;BYMONTH=6,7,8",
    "FREQ=MONTHLY;BYDAY=FR;BYMONTHDAY=13",
    "FREQ=MONTHLY;BYDAY=SA;BYMONTHDAY=7,8,9,10,11,12,13",
    "FREQ=YEARLY;INTERVAL=4;BYMONTH=11;BYDAY=TU;BYMONTHDAY=2,3,4,5,6,7,8",
    "FREQ=MONTHLY;COUNT=3;BYDAY=TU,WE,TH;BYSETPOS=3",
    "FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-2",
    "FREQ=HOURLY;INTERVAL=3;UNTIL19981224T080000Z",
    "FREQ=MINUTELY;INTERVAL=15;COUNT=6",
    "FREQ=MINUTELY;INTERVAL=20;BYHOUR=9,10,11,12,13,14,15,16",
    "FREQ=MINUTELY;BYHOUR=9,10,11,12,13,14,15,16;BYMINUTE=0,20,40",
    "FREQ=WEEKLY;INTERVAL=2;COUNT=4;BYDAY=TU,SU;WKST=MO"
    );

@exrules = (
    "FREQ=WEEKLY;COUNT=4;INTERVAL=2;BYDAY=TU,TH",
    "FREQ=DAILY;COUNT=10",
    "FREQ=YEARLY;COUNT=8;BYMONTH=6,7"
    );

@return_codes = (
    "2.0", "2.1", "2.2", "2.3", "2.4", "2.5", "2.6",
    "2.7", "2.8", "2.9", "2.10", "2.11",
    "3.0", "3.1", "3.2", "3.3", "3.4", "3.5", "3.6", "3.7",
    "3.8", "3.9", "3.10", "3.11", "3.12", "3.13", "3.14",
    "4.0",
    "5.0", "5.1", "5.2", "5.3"
    );

#############################################################
# Varios buttons and nobs that can tweek the output...
#############################################################
$recur = 0;		    # flag: allow recurring params
$rand_component_count = 0;  # flag: allow random number of components per ical obj
$date_cmd = "date";         # the date command
$hostname = "host";
$domain   = "example.com";
$bad_key_words = 0;         # 0 = no bad keywords, 1 = may generate bad keywords
$component_type = 1;        # component type to generate
$max_comp_type = 2;         # 1-Events,2-Freebusy
$SHOULD_prop_chances = 65;  # 0 - 100, % chance that SHOULD properties will appear
$MUST_prop_chances = 95;    # 0 - 100, % chance that MUST properties will appear
$MAY_prop_chances = 10;     # 0 - 100, % chance that MAY properties will appear
$RUIN_prop_chances = 10;    # 0 - 100, only valid when $bad_key_words is 1. 
			    #          it is the % chance that a keword will be bad
$max_fb_dur_hrs = 4;        # maximum number of hours that a busy time can be
                            #          the larger this number, the less the chances
			    #          of multiple busy times per day.

#############################################################
# ICALENDAR WRAPPER
#############################################################
&process_args();
&init();
&check_for_cgi_buttons();

if ($component_type == 2)
{
    # 0 = Publish, 5 = Reply
    $itip_method = $method[ ((int(rand(2)) == 0) ? 0 : 5) ];
}
else
{
    $itip_method = $method[int(rand(1 + $#method))];
}

printf("Content-Type:text/calendar; method=%s; charset=US-ASCII\n",$itip_method);
printf("Content-Transfer-Encoding: 7bit\n");
printf("\n");
printf("%s:%s\n", &ruin_keyword("BEGIN"), &ruin_keyword("VCALENDAR") );
printf("%s:-//seasnake/RandomICALgenerator//EN\n", &ruin_keyword("PRODID") );
printf("%s:2.0\n",&ruin_keyword("VERSION"));
printf("%s:%s\n",&ruin_keyword("METHOD"),$itip_method);

if ($component_type == 1)
{
    $numevents = 1;
    $numevents += int(rand(7)) if (1 == $rand_component_count);

    for ($iouter = 0; $iouter < $numevents; $iouter++)
    {
	&add_event();
    }
}
elsif ($component_type == 2)
{
    &add_freebusy();
}

printf("%s:%s\n", &ruin_keyword("END"), &ruin_keyword("VCALENDAR"));

0;


sub process_args
{
    while (@ARGV) {
        $arg = shift @ARGV;

        if ($arg eq '-d')
	{
            $debug = 1;
            print STDERR "Debug turned on...\n";
	}
	elsif ($arg eq '-r')
	{
	    $recur = 1;
            print STDERR "Recur turned on...\n" if ($debug == 1);
	}
	elsif ($arg eq '-rcc')
	{
	    $rand_component_count = 1;
            print STDERR "Random component count turned on...\n" if ($debug == 1);
	    
	}
	elsif ($arg eq '-bkw')
	{
	    $bad_key_words = 1;
            print STDERR "component_type = 2, freebusy...\n" if ($debug == 1);
	}
	elsif( $arg eq '-may')
	{
	    $arg = shift @ARGV;
	    $MAY_prop_chances = $arg;
	    $MAY_prop_chances = 0 if ($MAY_prop_chances < 0);
	    $MAY_prop_chances = 100 if ($MAY_prop_chances > 100);
            printf( STDERR "MAY_prop_chances = %d\n",$MAY_prop_chances)
		if ($debug == 1);
	}
	elsif( $arg eq '-must')
	{
	    $arg = shift @ARGV;
	    $MUST_prop_chances = $arg;
	    $MUST_prop_chances = 0 if ($MUST_prop_chances < 0);
	    $MUST_prop_chances = 100 if ($MUST_prop_chances > 100);
            printf( STDERR "MUST_prop_chances = %d\n",$MUST_prop_chances)
		if ($debug == 1);
	}
	elsif( $arg eq '-ruin')
	{
	    $arg = shift @ARGV;
	    $RUIN_prop_chances = $arg;
	    $RUIN_prop_chances = 0 if ($RUIN_prop_chances < 0);
	    $RUIN_prop_chances = 100 if ($RUIN_prop_chances > 100);
            printf( STDERR "RUIN_prop_chances = %d\n",$RUIN_prop_chances)
		if ($debug == 1);
	}
	elsif( $arg eq '-should')
	{
	    $arg = shift @ARGV;
	    $SHOULD_prop_chances = $arg;
	    $SHOULD_prop_chances = 0 if ($SHOULD_prop_chances < 0);
	    $SHOULD_prop_chances = 100 if ($SHOULD_prop_chances > 100);
            printf( STDERR "SHOULD_prop_chances = %d\n",$SHOULD_prop_chances)
		if ($debug == 1);
	}
	elsif ($arg eq '-fb')
	{
	    $component_type = 2;
            print STDERR "component_type = 2, freebusy...\n" if ($debug == 1);
	}
	elsif ($arg eq '-fbdh')
	{
	    $arg = shift @ARGV;
	    $max_fb_dur_hrs = $arg;
	    $max_fb_dur_hrs = 4 if ($max_fb_dur_hrs < 1);
            printf(STDERR "max_fb_dur_hrs = %d\n",$max_fb_dur_hrs)
		if ($debug == 1);
	}
	else
	{
            push(@mailto, $arg);
        }
    }
}

sub init
{
    srand();
    $uname = `uname -a`;
    if ($uname =~ /Windows/)
    {
	local($h);
	$date_cmd = "";
        $date_cmd = $ENV{'DATE_CMD'} if ($ENV{'DATE_CMD'} =~ /date/);
        $hostname = $ENV{'COMPUTERNAME'} unless !($ENV{'COMPUTERNAME'});
    }
    else
    {
	chop($hostname = `hostname`);
	chop($domain = `domainname`);
    }
}

#############################################################
# Build an indexed array.  The index is the property name,
# the value is the property value.
#############################################################
sub check_for_cgi_buttons
{
    @parms = split('&',$ENV{'QUERY_STRING'});
    # printf("<p>\n");
    foreach $val (@parms)
    {
	($p, $v) = split('=',$val);

	#printf("%s<br>\n", $val);
	#printf("p = %s,  v = %s<br>\n", $p, $v);

	# this filtering is a very cheap hack
	$v =~ s/(%40)/@/g;
	$v =~ s/\+/ /g;
	$v =~ s/%0D/\r/g;
	$v =~ s/%0A/\n /g;
	$vals{$p} = $v;
    }

    #
    # Now treat the button name just like the command line options...
    #
    $component_type = 2 if ( $vals{'btnFreeBusy'} eq "Random Freebusy" )
}

#############################################################
# Event
#   The SUMMARY may exceed 80 characters.
#   The end time may come before the start time.
#############################################################
sub add_event
{
    printf("%s:%s\n",&ruin_keyword("BEGIN"), &ruin_keyword("VEVENT"));

    &attach();
    &attendee(rand(10));
    &categories();
    &class();
    &comment();
    &contact();
    &created();
    &description();
    &dtstart_dtend_duration();
    &dtstamp();
    &geo();
    &last_modified();
    &location();
    &organizer();
    &priority();
    &relatedto();
    &resources();
    &recurrence();
    &sequence();
    &status();
    &summary("event");
    &transp();
    &uid();
    &url();
    &xprop();

    printf("%s:%s\n", &ruin_keyword("END"), &ruin_keyword("VEVENT"));
}

sub add_freebusy
{
    printf("%s:%s\n", &ruin_keyword("BEGIN"), &ruin_keyword("VFREEBUSY"));
    &attendee(1) if ($itip_method eq "REPLY");
    &comment();
    &contact();
    &dtstamp();
    &freebusy();
    &request_status();
    &organizer();
    &uid();
    &url();
    printf("%s:%s\n", &ruin_keyword("END"), &ruin_keyword("VFREEBUSY"));
}

sub attendee
{
    ($n) = @_;
    local($i);
    for ($i = 0; $i < $n; $i++)
    {
	printf( "%s", &ruin_keyword("ATTENDEE") );
	&cutype();
	&delegate();
	&member();
	&role();
	&partstat();
	&rsvp();
	&sentby();
	&common_name();
	&dir();
	&language();
	&xparam();
	printf( ":mailto:user$i\@example.com\n" );
    }
}

sub attach
{
    printf("%s:http://www.example%d.com/",&ruin_keyword("ATTACH"),int(rand(100)) )
	if ( int(rand(100)) < $SHOULD_prop_chances);
}

sub categories
{
    printf("%s:%s\n", &ruin_keyword("CATEGORIES"), $categories[int(rand(1 + $#categories))] )
	if ( int(rand(100)) < $SHOULD_prop_chances);
}

sub class
{
    printf("%s:%s\n", &ruin_keyword("CLASS"), $class[int(rand(1 + $#class))] )
	if ( int(rand(100)) < $SHOULD_prop_chances);
}

sub comment
{
    printf("%s:You are a weasel\n",&ruin_keyword("COMMENT") )
	if ( int(rand(100)) < $SHOULD_prop_chances);
}

sub common_name
{
    if ( int(rand(100)) < $MAY_prop_chances )
    {
	printf(";%s=\"%s\"", &ruin_keyword("CN"), &common_person_name());
    }
}

sub common_person_name
{
    $cnFirst[int(rand(1 + $#cnFirst))]." ".$cnLast[int(rand(1 + $#cnLast))];
}

sub contact
{
    local($r) = 1 + int(rand(100));
    if ( int(rand(100)) < 5 )
    {
	printf("%s",&ruin_keyword("CONTACT"));
	if ( 1 <= $r && $r < 33)
	{
	    printf(":Jim Dolittle\\, ABC Industries\\, +1-919-555-1234\n");
	}
	elsif (33 <= $r && $r < 67)
	{
	    printf(";ALTREP=\"CID=<part3.msg430523948750293487\@example.com>\":Jim Dolittle\\, ABC Industries\\, +1-919-555-1234\n");
	}
	else
	{
	    printf(";ALTREP=\"http://ld.example.com/pdi/jdo.vcf\":Jim Dolittle\\, ABC Industries\\, +1-919-555-1234\n");
	}
    }
}


sub created
{
    &rand_date(&ruin_keyword("CREATED")) if ( int(rand(100)) < $SHOULD_prop_chances );
}

sub cutype
{
    printf( ";%s=%s",&ruin_keyword("CUTYPE"),$cutypeparm[int(rand(1 + $#cutypeparm))] )
	if (int(rand(100)) < $MAY_prop_chances);
}

sub delegate
{
    if ( int(rand(100)) < $MAY_prop_chances )
    {
	printf( ";%s=\"mailto:user%d\@example.com\"",
	    &ruin_keyword("DELEGATED-FROM"), int(rand(25)) );
    }
    else
    {
	printf( ";%s=\"mailto:user%d\@example.com\"",
	    &ruin_keyword("DELEGATED-TO"), int(rand(25)) );
    }
}

sub description
{
    if ( int(rand(100)) < $SHOULD_prop_chances )
    {
	printf("DESCRIPTION:");
	$roll = int(rand(20));
	for ($i = 0 ; $i < $roll; $i++)
	{
	    printf("%sThis is the description line %d\n", ($i != 0) ? " " : "", $i);
	}
	if (0 == $roll)
	{
	    printf("\n");
	}
    }
}

sub dir
{
    if ( int(rand(100)) < $MAY_prop_chances )
    {
	printf(";%s=\"%s\"", &ruin_keyword("DIR"), &dir_name());
    }
}

sub dir_name
{
    "ldap://host.example.com:6666/o=eDABC%20Industries,c=3DUS??(cn=3DBJim%20Dolittle)"
}

sub dtstamp
{
    if ($date_cmd =~ /date/)
    {
	$ENV{'TZ'} = "GMT0";
	printf("%s:%s", &ruin_keyword("DTSTAMP"), `$date_cmd +19%y%m%dT%H%M%SZ`);
	$ENV{'TZ'} = "PST8PDT";
    }
    else
    {
        &rand_date(&ruin_keyword("DTSTAMP"));
    }
}

sub dtstart_dtend_duration
{
    &rand_date(&ruin_keyword("DTSTART"));

    #
    # 30% chance of a duration, otherwise it's dtend
    #
    if ( int(rand(100)) < 30 )
    {
	&rand_date(&ruin_keyword("DTEND"));
    }
    else
    {
	printf("%s",&ruin_keyword("DURATION"));
	printf(":P");
	local($d) = int(rand(2));
#	if ($w > 0)
#	{
#	    printf("%dW",$d);
#	}
	$d = int(rand(3));
	if ($d > 0)
	{
	    printf("%dD",$d);
	}
	printf("T%dH%dM%dS\n",
	    int(rand(40)),              # hours
	    int(rand(70)),              # minutes
	    int(rand(72))
	    );
    }
}

sub fbtypeparam
{
    local($fbtype,$r);

    if ( int(rand(100)) < $MAY_prop_chances )
    {
	if ( int(rand(100)) < 30 )
	{
	    $fbtype = $fbtypeparm[int(rand(1 + $#fbtypeparm))];
	}
	else
	{
            $r = int(rand(1000));
	    $fbtype = "X-" . "$r";
	}
	printf(";%s=%s", &ruin_keyword("FBTYPE"), $fbtype );
    }
}

sub freebusy
{
    local($year,$month,$day,$hour,$min,$dur_wk,$dur_day,$dur_hr,$dur_min,
	  $i,$j,$count,$daycount);
    #---------------------------------------------------
    # first compute DTSTART
    #---------------------------------------------------
    $year = 1998;
    $month = 1 + int(rand(12));
    $day = 1 + int(rand(28));

    $dur_wk = 0;
    $dur_day = 0;
    $dur_hr = int(rand(24));
    $dur_min = int(rand(60));

    printf("%s:%04d%02d%02dT%02d%02d00Z\n",
	    &ruin_keyword("DTSTART"),
	    $year, $month, $day,
	    $hour, $min
	    );
    --$hour if ($hour > 1);

    #---------------------------------------------------
    # The first busy time need not fall on DTSTART...
    #---------------------------------------------------
    if ($SHOULD_prop_chances > 10)
    {
	#
	# yea, the stuff below is a real hack...
	#
	$min += $dur_min + int(rand(60));
	$hour += int($min / 60);
	$min %= 60;
	$hour += dur_hr + int(rand(24));
	$day += int($hour / 24);
	$hour %= 24;
	$day += dur_day + int(rand(29));
	$month += int($day/28);
	$day %= 28;
	$day++;
	$year += int($month / 12);
	$month %= 12;
	++$month;
    }

    print &ruin_keyword("FREEBUSY");
    print &fbtypeparam();
    print ":";
    # printf("%s%s:", &ruin_keyword("FREEBUSY"), &fbtypeparam() );
    $count = 1 + int(rand(10));
    $j = 0;
    for ( $i= 0; $i < $count; $i++)
    {
	$daycount = 0;

	while ($daycount == 0 || $hour < 23)
	{
	    $dur_hr = int(rand(8));
	    $dur_min = int(rand(60));
	    # printf("%s%04d%02d%02dT%02d%02d00Z/P%dW%dDT%dH%dM",
	    printf("%s%04d%02d%02dT%02d%02d00Z/P%dDT%dH%dM\n",
		($j == 0) ? "" : " ,",
		$year, $month, $day,
		$hour, $min,
	 	# $dur_wk,
		$dur_day,
		$dur_hr, $dur_min
		);
	    ++$daycount;
	    $hour += int(rand(3)) + $dur_hr;
	    $min += int(rand(45)) + $dur_min;
	    $hour += int($min/60);
	    $min = $min % 60;
	    ++$j;  # the number of periods printed
	}

	#
	# yea, the stuff below is a real hack...
	#
	$min += $dur_min + int(rand(60));
	$hour += int($min / 60);
	$min %= 60;
	$hour += dur_hr + int(rand(24));
	$hour += dur_hr + int(rand(24));
	$day += int($hour / 24);
	$hour %= 24;
	$day += dur_day + int(rand(29));
	--$month;
	$month += int($day/28);
	$day %= 28;
	$day++;
	$year += int($month / 12);
	$month %= 12;
	++$month;
    }
    # printf("\n");
    printf("%s:%04d%02d%02dT%02d%02d00Z\n",
	    &ruin_keyword("DTEND"),
	    $year, $month, $day,
	    $hour, $min
	    );
}

sub geo
{
    printf("%s:%f,%f\n", &ruin_keyword("GEO"), rand(360), rand(360))
	if ( int(rand(100)) < $MAY_prop_chances );
}

sub language
{
    printf( ";%s=en",&ruin_keyword("LANGUAGE") )
	if ( int(rand(100)) < $MAY_prop_chances );
}

sub last_modified
{
    &rand_date(&ruin_keyword("LAST-MODIFIED"))
	if ( int(rand(100)) < $MAY_prop_chances );
}

sub location
{
    printf("%s:Conference Room %d\n", &ruin_keyword("LOCATION"), rand(10)+1 )
	if ( int(rand(100)) < $SHOULD_prop_chances );
}

sub member
{
    printf( ";%s=\"mailto:group%d\@example.com\"",
	&ruin_keyword("MEMBER"),int(rand(25)) )
            if ( int(rand(100)) < $MAY_prop_chances );
}

sub organizer
{
    if ( int(rand(100)) < $MUST_prop_chances )
    {
	printf( &ruin_keyword("ORGANIZER"));
	&sentby() if ( int(rand(100)) < $MAY_prop_chances );
	printf("%s", &common_name()) 
	    if ( int(rand(100)) < $MAY_prop_chances );
	printf(";DIR=\"%s\"", &dir_name())
	    if ( int(rand(100)) < $MAY_prop_chances );
	printf("%s", &language()) if ( int(rand(100)) < $MAY_prop_chances );
	printf("%s", &x_param()) if ( int(rand(100)) < $MAY_prop_chances );
	printf( ":mailto:user%d\@example.com\n",int(rand(100)) );
    }
}

sub partstat
{
    printf( ";%s=%s",&ruin_keyword("PARTSTAT"),$partstatparm[int(rand(1 + $#partstatparm))] )
	if (int(rand(100)) < $SHOULD_prop_chances);
}

sub priority
{
    printf("%s:%d\n", &ruin_keyword("PRIORITY"), 1 + int(rand(12)))
	if (int(rand(100)) < $MAY_prop_chances);
}

sub rand_date
{
    printf("%s:%4d%02d%02dT%02d%02d%02dZ\n", 
	    @_,
	    1998 + int(rand(3)),    # yr:  1998 - 2001
	    1 + int(rand(12)),	    # mon: 1 - 12
	    1 + int(rand(28)),      # day: 1 - 28
	    int(rand(24)),	    # hr:  0 - 23
	    int(rand(60)),	    # min: 0 - 59
	    int(rand(60))	    # sec: 0 - 59
	    );
}

sub recurrence
{
    if ($recur != 0)
    {
	local($i,$roll);
	if ( int(rand(100)) < 50 )
	{
	    printf("%s:%s\n",&ruin_keyword("RRULE"),$rrules[int(rand(1 + $#rrules))]);
	}
	if ( int(rand(100)) < 50 )
	{
	    printf("%s:%s\n",&ruin_keyword("EXRULE"),$exrules[int(rand(1 + $#exrules))]);
	}
	if ( int(rand(100)) < 50 )
	{
	    $roll = int(rand(20));
	    for ($i = 0 ; $i < $roll; $i++)
	    {
		&rand_date(&ruin_keyword("RDATE"));
	    }
	}
	if ( int(rand(100)) < 50 )
	{
	    $roll = int(rand(20));
	    for ($i = 0 ; $i < $roll; $i++)
	    {
		&rand_date(&ruin_keyword("EXDATE"));
	    }
	}
    }
}

sub relatedto
{
    printf("%s:%s\n", &ruin_keyword("RELATED-TO"), &unique_id())
	if (int(rand(100)) < $MAY_prop_chances);
}

sub request_status
{
    printf("%s:%s\n",
	&ruin_keyword("REQUEST-STATUS"),
	$request_codes[int(rand(1 + $#request_codes))])
	    if (int(rand(100)) < $MAY_prop_chances );
}

sub resources
{
    printf("%s:%s\n", &ruin_keyword("RESOURCES"), $resources[int(rand($#resources))] )
	if (int(rand(100)) < $MAY_prop_chances );
}

sub role
{
    printf( ";%s=%s",&ruin_keyword("ROLE"), $roleparm[int(rand(1 + $#roleparm))] )
	if (int(rand(100)) < $SHOULD_prop_chances);
}

sub rsvp
{
    printf( ";RSVP=%s", (int(rand(100)) < 50) ? "TRUE" : "FALSE")
	if (int(rand(100)) < $MUST_prop_chances);
}

sub sentby
{
    printf( ";%s=\"mailto:user%d\@example.com\"", 
	&ruin_keyword("SENT-BY"), int(rand(25)) )
	    if ( int(rand(100)) < $MAY_prop_chances );
}

sub sequence
{
    printf( "%s:0\n",&ruin_keyword("SEQUENCE"));
}

sub status
{
    printf("%s:%s\n", &ruin_keyword("STATUS"),$status[int(rand(1 + $#status))] )
	if (int(rand(100)) < $SHOULD_prop_chances );
}

sub summary
{
    printf("%s:This is a ",&ruin_keyword("SUMMARY"));
    $roll = int(rand(15));
    for ($i = 0 ; $i < $roll; $i++)
    {
	printf("really ");
    }
    printf("stupid %s\n",@_);
}

sub transp
{
    printf("%s:%s\n", &ruin_keyword("TRANSP"),$transparm[int(rand(1 + $#transparm))] )
	if (int(rand(100)) < $MAY_prop_chances );
}

sub uid
{
    printf( "%s:%s\n", &ruin_keyword("UID"),&unique_id() )
	 if ( int(rand(100)) < $MUST_prop_chances );
}

sub unique_id
{
    local($d,$r);
    if ($date_cmd =~ /date/)
    {
	chop($d = `$date_cmd +%y%m%d%H%M%S` );
    }
    else
    {
	$d = rand(711771);
    }
    $r = int(rand(100000));
    $hostname.".".$domain.$d."$$"."$r";
}

sub url
{
    printf( "%s:http://cal.example.com/pub/calendar/jsmith/%s/freetime.vcs\n",
        &ruin_keyword("URL"),
	$cnLast[int(rand(1 + $#cnLast))] ) 
	    if (int(rand(100)) < $MAY_prop_chances );
}

sub x_param
{
    $r = int(rand(1000));
    ";".&ruin_keyword("X-PARAM").$r;
}

sub xparam
{
    printf(";%s%d=%d", &ruin_keyword("X-PARAM"), int(rand(100)), int(rand(100)))
	if ( int(rand(100)) < $MAY_prop_chances );
}

sub xprop
{
    local($i,$n);
    if (int(rand(100)) < 5 )
    {
	printf("%s%d:",&ruin_keyword("X-PROP"),int(rand(100)));
	$n = 1 +int(rand(20));
	for ($i = 0; $i < $n; $i++)
	{
	    printf("blah ");
	}
	printf("\n");
    }
}

sub ruin_keyword
{
    local($k) = @_;
    if (1 == $bad_key_words)
    {
	local($i) = int(rand(100));
	$r = "QQ";
	if (int(rand(100)) < $RUIN_prop_chances)
	{
	    if ( $i < 50 )
	    {
		$k .= $r;
	    }
	    else
	    {
		$k = $r . $k;
	    }
	}
    }

    $k;
}

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
  

use Socket;

$mailhost = 'localhost';

&process_args;

print "Content-Type: text/html\n\n";
print "<HTML><BODY>\n";

#
# Build an indexed array.  The index is the property name,
# the value is the property value.
#
@parms = split('&',$ENV{'QUERY_STRING'});
printf("<p>\n");
foreach $val (@parms)
{
    #printf("%s<br>\n", $val);
    #printf("p = %s,  v = %s<br>\n", $p, $v);
    ($p, $v) = split('=',$val);
    $v =~ s/(%40)/@/g;
    $v =~ s/\+/ /g;
    $v =~ s/%0D/\r/g;
    $v =~ s/%0A/\n /g;
    $vals{$p} = $v;
}

printf("<B>Busy time request sent:</b><p>\n");
printf("Organizer:  %s\n<br>\n", $vals{'organizer'} );
printf("message sent to:  %s\n<br>\n", $vals{'who'} );
printf("start time: %s\n<br>\n", $vals{'DTSTART'} );
printf("end time: %s\n<br>\n", $vals{'DTEND'} );

@mailto = split(' ',$vals{'who'});
printf("sending the mail message now.\n");

print "\n</pre></body></html>";

if ($flag_debug )
{
    print STDERR "----------------------------------------------\n";
    print STDERR "LOGINFO:\n";
    print STDERR " mailto: \@mailto\n";
    print STDERR "----------------------------------------------\n";
}


&mail_the_message;
0;

sub process_args
{
    while (@ARGV) {
        $arg = shift @ARGV;

        if ($arg eq '-d')
	{
            $flag_debug = 1;
            print STDERR "Debug turned on...\n";
	}
	elsif ($arg eq '-h')
	{
	    $mailhost = shift @ARGV;
	}
	else
	{
            push(@mailto, $arg);
        }
    }
}


sub get_response_code {
    my ($expecting) = @_;
#     if ($flag_debug) {
# 	print STDERR "SMTP: Waiting for code $expecting\n";
#     }
    while (1) {
	my $line = <S>;
# 	if ($flag_debug) {
# 	    print STDERR "SMTP: $line";
# 	}
	if ($line =~ /^[0-9]*-/) {
	    next;
	}
	if ($line =~ /(^[0-9]*) /) {
	    my $code = $1;
	    if ($code == $expecting) {
# 		if ($flag_debug) {
# 		    print STDERR "SMTP: got it.\n";
# 		}
		return;
	    }
	    die "Bad response from SMTP -- $line";
	}
    }
}
	    
    
sub mail_the_message {
    chop(my $hostname = `hostname`);

    my ($remote,$port, $iaddr, $paddr, $proto, $line);

    $remote  = $mailhost;
    $port    = 25;
    if ($port =~ /\D/) { $port = getservbyname($port, 'tcp') }
    die "No port" unless $port;
    $iaddr   = inet_aton($remote)               || die "no host: $remote";
    $paddr   = sockaddr_in($port, $iaddr);

    $proto   = getprotobyname('tcp');
    socket(S, PF_INET, SOCK_STREAM, $proto)  || die "socket: $!";
    connect(S, $paddr)    || die "connect: $!";
    select(S); $| = 1; select(STDOUT);

    get_response_code(220);
    print S "HELO $hostname\n";
    get_response_code(250);
    printf( S "MAIL FROM: %s\n", $vals{'organizer'} );
    get_response_code(250);
    foreach $i (@mailto) {
	print S "RCPT TO: $i\n";
	get_response_code(250);
    }
    print S "DATA\n";
    get_response_code(354);

    printf(S "Date: %s", `date`);
    printf(S "From: %s\n", $vals{'organizer'});
    printf(S "Return-Path: <%s>\n", $vals{'organizer'} );
    printf(S "To: %s\n",$vals{'who'} );
    printf(S "Subject:Calendar Busy Time Request\n");
    printf(S "Mime-Version: 1.0\n");
    printf(S "Content-Type:text/calendar; method=REQUEST; charset=US-ASCII\n");
    printf(S "Content-Transfer-Encoding: 7bit\n");
    printf(S "\n");
    printf(S "BEGIN:VCALENDAR\n");
    printf(S "PRODID:-//seasnake/DesktopCalendar//EN\n");
    printf(S "METHOD:REQUEST\n");
    printf(S "VERSION:2.0\n");
    printf(S "BEGIN:VFREEBUSY\n");
    printf(S "ORGANIZER:Mailto:%s\n",$vals{'organizer'});
    printf(S "ATTENDEE:Mailto:%s\n",$vals{'who'});
    $ENV{'TZ'} = "GMT0";
    printf(S "DTSTAMP:%s",`date +19%y%m%dT%H%M%SZ`);
    $ENV{'TZ'} = "PST8PDT";
    printf(S "DTSTART:%s\n",$vals{'DTSTART'});
    printf(S "DTEND:%s\n",$vals{'DTEND'});
    chop($h = `hostname`);
    chop($dm = `domainname`);
    chop($d = `date +%y%m%d%H%M%S`);
    printf(S "UID:%s.%s-%s.%s\n",$h, $dm, $d, "$$" );
    printf(S "END:VFREEBUSY\n");
    printf(S "END:VCALENDAR\n");

    print S ".\n";
    get_response_code(250);
    print S "QUIT\n";

    close(S);
}

#!/usr/local/bin/perl
%types=("x2j.in",1,"x2j.out",2,"x2j.inout",3,"x2j.ret",4,
        "j2x.in",5,"j2x.out",6,"j2x.inout",7,"j2x.ret",8);

sub usage {
    print("##################################################\n");
    print("Usage: resGen.pl <type> <log_dir> <output_file>\n");
    print("       Where type is one of following: ".join(" ",keys(%types)). "\n");
    print("       or \"all\"\n");
    print("       log_dir is directory with BlackConnect test logs\n");
    print("       output_file is output HTML file name\n");
    print("##################################################\n");
    exit(-1);
}

#Main
unless ($#ARGV == 2 ) {
    usage();
}
$type=$ARGV[0];
$log_dir=$ARGV[1];
$output_file=$ARGV[2];
(-d $log_dir) || die ("ERROR: $log_dir doesn't exist !\n");
if($types{$type}) {
    $output = "<html><body bgcolor=white>\n";
    if($type =~ /.*inout/) {
	$output .= DoInout();
    } else {
	$output .= DoUsual();
    }
    $output.="</body></html>\n";
    save($output);
} elsif($type eq "all") {
    $common_output = "<html><body bgcolor=white>\n";
    foreach $type (sort(keys(%types))) {
	    if($type =~ /.*inout/) {
		$common_output .= DoInout();
	    } else {
		$common_output .= DoUsual();
	    }
    } 
    $common_output.="</body></html>\n";
    save($common_output);
} else {
    print "Unknown type $type\n\n";
    usage();
}
sub save {
  my $output = shift(@_);
  open(OUT,">$output_file") || die "Can't open output file $!\n";
  print OUT $output;
  close(OUT);
}
sub DoUsual {
    my @server_files=<$log_dir/$type.server.*>;
    my @client_files=<$log_dir/$type.client.*>;
    my %s_files=split(/=/,join("=1=",@server_files)."=1");
    my %c_files=split(/=/,join("=1=",@client_files)."=1");
    my $all_log="<H1>Results for <B>$type</B> tests</H1><table border=1 bgcolor=lightblue><br>";

	if ($type eq "x2j.in") {
		$all_log.="Client part of the X2JIN component is written in C++.<br>";
		$all_log.="Server part of the X2JIN component is written in Java.<br>";
		$all_log.="<b>How it works:</b> Client part is loaded by Mozilla browser and loads server part of component. After that, client part sets different parameters and tries to invoke methods with these parameters from the server part.<br>";
		$all_log.="<b>Expected result:</b> Parameters, which were set in client part, are transferred to the server part correctly.<br><br>";
	}

	if ($type eq "x2j.out") {
		$all_log.="Client part of the X2JOUT component is written in C++.<br>"; 
		$all_log.="Server part of the X2JOUT component is written in Java.<br>"; 
		$all_log.="<b>How it works:</b> Client part is loaded by Mozilla browser and loads server part of component. After that, client part gets different parameters invoking methods from the server part.<br>"; 
		$all_log.="<b>Expected result:</b> Parameters, which were set in server part, are transferred to the client part correctly.<br><br>"; 
	}

	if ($type eq "x2j.ret") {
		$all_log.="Client part of the X2JRET component is written in C++.<br>"; 
		$all_log.="Server part of the X2JRET component is written in Java.<br>"; 
		$all_log.="<b>How it works:</b> Client part is loaded by Mozilla browser and loads server part of component. After that, client part gets different parameters invoking methods from the server part, using retval.<br>"; 
		$all_log.="<b>Expected result:</b> Parameters, which were set in server part, are transferred to the client part correctly.<br><br>";
	}
	
	if ($type eq "j2x.in") {
		$all_log.="Client part of the J2XIN component is written in Java.<br>"; 
		$all_log.="Server part of the J2XIN component is written in C++.<br>"; 
		$all_log.="<b>How it works:</b> Client part is loaded by Mozilla browser and loads server part of component. After that, client part sets different parameters and tries to invoke methods with these parameters from the server part.<br>"; 
		$all_log.="<b>Expected result:</b> Parameters, which were set in client part, are transferred to the server part correctly.<br><br>";
	}

	if ($type eq "j2x.out") {
		$all_log.="Client part of the J2XOUT component is written in Java.<br>"; 
		$all_log.="Server part of the J2XOUT component is written in C++.<br>"; 
		$all_log.="<b>How it works:</b> Client part is loaded by Mozilla browser and loads server part of component. After that, client part gets different parameters invoking methods from the server part.<br>"; 
		$all_log.="<b>Expected result:</b> Parameters, which were set in server part, are transferred to the client part correctly.<br><br>"; 
	}

	if ($type eq "j2x.ret") {
		$all_log.="Client part of the J2XRET component is written in Java.<br>"; 
		$all_log.="Server part of the J2XRET component is written in C++.<br>"; 
		$all_log.="<b>How it works:</b> Client part is loaded by Mozilla browser and loads server part of component. After that, client part gets different parameters invoking methods from the server part, using retval.<br>"; 
		$all_log.="<b>Expected result:</b> Parameters, which were set in server part, are transferred to the client part correctly.<br><br>"; 
	}

    if($#client_files == -1) {
	return "";
    }
    if($#server_files != $#client_files) {
	print "Warning!!!. Different amount of server and client files\n";
    }
    
    $all_log.="<tr bgcolor=lightgreen><td>ID</td><td>Client data</td><td>Server data</td><td>Result</td><td>Comments</td></tr>";
    foreach $c_file (sort(keys(%c_files))) {
	@CF=();
	@SF=();
	$result="UNDEF";
	$c_file =~ /.*(\..*$)/;
	$ID=$1;
        open(C,$c_file);
	@CF=<C>;
	close(C);
        unless($s_files{$log_dir."/".$type.".server".$ID}) {
	    print "Warning!!!. Server result for $log_dir\/$type.server$ID doesn't exist\n";
	    $result = "FAILED";
	}else {
	    open(S,$log_dir."/".$type.".server".$ID)||die "Error opening server file\n";
	    $i=0;
	    $result="PASSED";
	    while($line=<S>) {
		$line =~ s/\r|\n|^\s*|\s*$//g;
		$CF[$i]=~ s/\r|\n|^\s*|\s*$//g;
		$SF[$i]=$line;

		if (($ID eq ".float")||($ID eq ".double")) {
		   unless (substr($SF[$i],0,3) eq substr($CF[$i],0,3))
			 {$result="FAILED";}
		} else {
		    unless($line eq $CF[$i]) {
		    	$result="FAILED";
		    }
		}

		$i++;
	    }
	    close(S);
	} 
        $all_log.="<tr><td>$type$ID</td><td>".join("<BR>",@CF)."</td><td>".join("<BR>",@SF)."</td>";
	if($result=~/FAILED/) {
	    $all_log.="<td bgcolor=red>".$result."</td>";
	} else {
	    $all_log.="<td>".$result."</td>";
	}

	if ($ID eq ".boolean"){ $all_log.="<td>Test for PRBool</td>";}
	if ($ID eq ".char"){	$all_log.="<td>Test for char</td>";}
	if ($ID eq ".wchar"){	$all_log.="<td>Test for PRUnichar</td>";}
	if ($ID eq ".charArray"){	$all_log.="<td>Test for char array</td>";}
	if ($ID eq ".string"){	$all_log.="<td>Test for char*</td>";}
	if ($ID eq ".wstring"){	$all_log.="<td>Test for PRUnichar*</td>";}
	if ($ID eq ".float"){	$all_log.="<td>Test for float</td>";}
	if ($ID eq ".double"){	$all_log.="<td>Test for double</td>";}
	if ($ID eq ".octet"){	$all_log.="<td>Test for PRUint8</td>";}
	if ($ID eq ".short"){	$all_log.="<td>Test for PRInt16</td>";}
	if ($ID eq ".ushort"){	$all_log.="<td>Test for PRUint16</td>";}
	if ($ID eq ".long"){	$all_log.="<td>Test for PRInt32</td>";}
	if ($ID eq ".ulong"){	$all_log.="<td>Test for PRUint32</td>";}
	if ($ID eq ".longlong"){$all_log.="<td>Test for PRInt64</td>";}
	if ($ID eq ".ulonglong"){	$all_log.="<td>Test for PRUint64</td>";}
	if ($ID eq ".stringArray"){	$all_log.="<td>Test for char* array</td>";}
	if ($ID eq ".longArray"){	$all_log.="<td>Test for PRInt32 array</td>";}
	if ($ID eq ".mixed"){	$all_log.="<td>Test for misc values</td>";}
	if ($ID eq ".object"){	$all_log.="<td>Test for interface*</td>";}
	if ($ID eq ".iid"){	$all_log.="<td>Test for const nsIID&</td>";}
	if ($ID eq ".cid"){	$all_log.="<td>Test for const nsCID& </td>";}

	$all_log.="</tr>\n";
    }
$all_log.="</table>\n";
return $all_log;
}


sub DoInout {

    my @server_files=<$log_dir/$type.server.*>;
    my @client_files=<$log_dir/$type.client.*>;
    my @xclient_files=<$log_dir/$type.xclient.*>;
    my %s_files=split(/=/,join("=1=",@server_files)."=1");
    my %c_files=split(/=/,join("=1=",@client_files)."=1");
    my %xc_files=split(/=/,join("=1=",@xclient_files)."=1");
    my $all_log="<H1>Results for <B>$type</B> tests</H1><table border=1 bgcolor=lightblue>\n";
    if($#server_files != $#client_files) {
	print "Warning!!!. Different amount of server and client files\n";
    }
    
    $all_log.="<tr bgcolor=lightgreen><td>ID</td><td>Client data</td><td>Server data</td><td>XClient data</td><td>Result</td><td>Comments</td></tr>";

	if ($type eq "x2j.inout") {
		$all_log.="Client part of the X2JINOUT component is written in C++.<br>"; 
		$all_log.="Server part of the X2JINOUT component is written in Java.<br>";
		$all_log.="<b>How it works:</b> Client part is loaded by Mozilla browser and loads server part of component. After that, client part sets different parameters and transfers them to the server part. The server part transfers these parameters back to the client part.<br>"; 
		$all_log.="<b>Expected result:</b> Parameters, which were set in the client part, are transferred to the server part and back to the client part correctly.<br><br>"; 
	}

	if ($type eq "j2x.inout") {
		$all_log.="Client part of the J2XINOUT component is written in Java.<br>"; 
		$all_log.="Server part of the J2XINOUT component is written in C++.<br>"; 
		$all_log.="<b>How it works:</b> Client part is loaded by Mozilla browser and loads server part of component. After that, client part sets different parameters and transfers them to the server part. The server part transfers these parameters back to the client part.<br>"; 
		$all_log.="<b>Expected result:</b> Parameters, which were set in the client part, are transferred to the server part and back to the client part correctly.<br><br>"; 
	}

    foreach $c_file (sort(keys(%c_files))) {
	@CF=();
	@XCF=();
	@SF=();
@TMP=();	
	$result="UNDEF";
	$c_file =~ /.*(\..*$)/;
	$ID=$1;
        open(C,$c_file);
	@CF=<C>;
	close(C);
        unless($s_files{$log_dir."/".$type.".server".$ID}) {
	    print "Warning!!!. Server result for $log_dir\/$type.server$ID doesn't exist\n";
	    $result = "FAILED";    
	}else {
	    open(S,$log_dir."/".$type.".server".$ID)||die "Error opening server file\n";
	    @SF=<S>;
	    close(S);
	}
	unless($xc_files{$log_dir."/".$type.".xclient".$ID}) {
	    print "Warning!!!. Second client result for $log_dir\/$type.xclient$ID doesn't exist\n";
	    $result = "FAILED";    
	}else {
	    open(XC,$log_dir."/".$type.".xclient".$ID)||die "Error opening second client file\n";
	    @XCF=<XC>;
	    close(XC);
	}

	$result="PASSED";
	if (($ID eq ".float")||($ID eq ".double")) {

 	      	for($i=0;$i<=$#CF;$i++) {
		    $XCF[$i] =~ s/\r|\n|^\s*|\s*$//g;
		    $CF[$i]  =~ s/\r|\n|^\s*|\s*$//g;
		    $SF[$i]  =~ s/\r|\n|^\s*|\s*$//g;

		  unless ((substr($SF[$i],0,3) eq substr($CF[$i],0,3))
	                &&(substr($XCF[$i],0,3) eq substr($CF[$i],0,3))) {
		 	  $result="FAILED";
	          }
        	 }
	} else {
	        for($i=0;$i<=$#CF;$i++) {
  		    $XCF[$i] =~ s/\r|\n|^\s*|\s*$//g;
		    $CF[$i]  =~ s/\r|\n|^\s*|\s*$//g;
		    $SF[$i]  =~ s/\r|\n|^\s*|\s*$//g;
		    unless(($CF[$i] eq $XCF[$i])&&($CF[$i] eq $SF[$i])) {
			$result="FAILED";
		    }
		} 
	}

 
       $all_log.="<tr><td>$type$ID</td><td>".join("<BR>",@CF)."</td><td>".join("<BR>",@SF)."</td><td>".join("<BR>",@XCF)."</td>";
	if($result=~/FAILED/) {
	    $all_log.="<td bgcolor=red>".$result."</td>";
	} else {
	    $all_log.="<td>".$result."</td>";
	}

	if ($ID eq ".boolean"){ $all_log.="<td>Test for PRBool</td>";}
	if ($ID eq ".char"){	$all_log.="<td>Test for char</td>";}
	if ($ID eq ".wchar"){	$all_log.="<td>Test for PRUnichar</td>";}
	if ($ID eq ".charArray"){	$all_log.="<td>Test for char array</td>";}
	if ($ID eq ".string"){	$all_log.="<td>Test for char*</td>";}
	if ($ID eq ".wstring"){	$all_log.="<td>Test for PRUnichar*</td>";}
	if ($ID eq ".float"){	$all_log.="<td>Test for float</td>";}
	if ($ID eq ".double"){	$all_log.="<td>Test for double</td>";}
	if ($ID eq ".octet"){	$all_log.="<td>Test for PRUint8</td>";}
	if ($ID eq ".short"){	$all_log.="<td>Test for PRInt16</td>";}
	if ($ID eq ".ushort"){	$all_log.="<td>Test for PRUint16</td>";}
	if ($ID eq ".long"){	$all_log.="<td>Test for PRInt32</td>";}
	if ($ID eq ".ulong"){	$all_log.="<td>Test for PRUint32</td>";}
	if ($ID eq ".longlong"){$all_log.="<td>Test for PRInt64</td>";}
	if ($ID eq ".ulonglong"){	$all_log.="<td>Test for PRUint64</td>";}
	if ($ID eq ".stringArray"){	$all_log.="<td>Test for char* array</td>";}
	if ($ID eq ".longArray"){	$all_log.="<td>Test for PRInt32 array</td>";}
	if ($ID eq ".mixed"){	$all_log.="<td>Test for misc values</td>";}
	if ($ID eq ".object"){	$all_log.="<td>Test for interface*</td>";}
	if ($ID eq ".iid"){	$all_log.="<td>Test for const nsIID&</td>";}
	if ($ID eq ".cid"){	$all_log.="<td>Test for const nsCID& </td>";}

	$all_log.="</tr>\n";
    }
$all_log.="</table>\n";
return $all_log;
}

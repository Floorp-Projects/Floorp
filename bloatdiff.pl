#!perl -w

$OLDFILE = $ARGV[0];
$NEWFILE = $ARGV[1];

sub processFile {
    local ($FILENAME, $map) = @_;
    open(FH, $FILENAME);
    while (<FH>) {
        if (/^\s+(\d+) (\w+)\s+([\-\d]+)\s+([\-\d]+)\s+([\-\d]+)\s+([\-\d]+) \(\s+([\-\d\.]+) \+\/\-\s+([\-\d\.]+)\)\s+([\-\d]+)\s+([\-\d]+) \(\s+([\-\d\.]+) \+\/\-\s+([\-\d\.]+)\)/) {
            $name = $2;
            $size = $3;
            $leaked = $4;
            $objTotal = $5;
            $objRem = $6;
            $objMean = $7;
            $objStdDev = $8;
            $refTotal = $9;
            $refRem = $10;
            $refMean = $11;
            $refStdDev = $12;
            $$map{$name} = { name=>$name,
                             size=>$size,
                             leaked=>$leaked,
                             objTotal=>$objTotal,
                             objRem=>$objRem,
                             objMean=>$objMean,
                             objStdDev=>$objStdDev,
                             refTotal=>$refTotal,
                             refRem=>$refRem,
                             refMean=>$refMean,
                             refStdDev=>$refStdDev };
        }
    }
    close(FH);
}

%oldMap = ();
processFile($OLDFILE, \%oldMap);

%newMap = ();
processFile($NEWFILE, \%newMap);

################################################################################

sub leaksDelta {
    local ($key) = @_;
    local $oldLeaks = $oldMap{$key}{leaked};
    local $newLeaks = $newMap{$key}{leaked};
    local $percentLeaks = 0;
    if (defined($oldLeaks) && $oldLeaks != 0) {
        $percentLeaks = ($newLeaks - $oldLeaks) / $oldLeaks * 100;
    }
    return ($newLeaks, $percentLeaks);
}
    
################################################################################

sub bloatDelta {
    local ($key) = @_;
    local $oldSize = $oldMap{$key}{size};
    if (!defined($oldSize)) {
        $oldSize = 0;
    }
    local $oldTotal = $oldMap{$key}{objTotal};
    if (!defined($oldTotal)) {
        $oldTotal = 0;
    }
    local $oldBloat = $oldTotal * $oldSize;
    local $newBloat = $newMap{$key}{objTotal} * $newMap{$key}{size};
    local $percentBloat = 0;
    if (defined($oldBloat) && $oldBloat != 0) {
        $percentBloat = ($newBloat - $oldBloat) / $oldBloat * 100;
    }
    return ($newBloat, $percentBloat);
}

################################################################################

if ($NEWFILE =~ /all\-(\d+)\-(\d+)\-(\d+)\-(\d{2})(\d{2})(\d{2}).txt/) {
    $summary = "delta-$1-$2-$3-$4$5$6.txt";
    #$newDate = "$2/$3/$1 $4:$5:$6";
    if ($OLDFILE =~ /all\-(\d+)\-(\d+)\-(\d+)\-(\d{2})(\d{2})(\d{2}).txt/) {
        #$oldDate = "$2/$3/$1 $4:$5:$6";
        open(SF, ">$summary");
        print SF "Bloat/Leak Delta Report\n";
        print SF "Current file:  $NEWFILE\n";
        print SF "Previous file: $OLDFILE\n";
        print SF "--------------------------------------------------------------------------\n";
        
        %keys = ();
        @keys = sort { $newMap{$b}{leaked} <=> $newMap{$a}{leaked}
                   ||  $newMap{$b}{size} * $newMap{$b}{objTotal}
                       <=> $newMap{$a}{size} * $newMap{$a}{objTotal}} keys %newMap;
        print SF sprintf "%-20s %10s  %10s %10s  %10s\n", ("CLASS", "LEAKS", "delta", "BLOAT", "delta");
        print SF "--------------------------------------------------------------------------\n";
        foreach $key (@keys) {
            local ($newLeaks, $percentLeaks) = leaksDelta($key);
            local ($newBloat, $percentBloat) = bloatDelta($key);
            print SF sprintf "%-20s %10s %10.2f%% %10s %10.2f%%\n",
                  ($key, $newLeaks, $percentLeaks, $newBloat, $percentBloat);
        }

        close(SF);
    }
}

################################################################################

print sprintf "<a href=$summary>leaks: %10d %+8.2f%%<br>\n", leaksDelta("TOTAL");
print sprintf "bloat: %10d %+8.2f%%</a>\n", bloatDelta("TOTAL");


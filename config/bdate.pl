($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime;
printf("500%02d%03d%02d\n", $year, 1+$yday, $hour);

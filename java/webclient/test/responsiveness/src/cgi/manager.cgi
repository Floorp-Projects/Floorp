#!<PERL_DIR>/perl
use CGI;
#/*
# * The contents of this file are subject to the Mozilla Public
# * License Version 1.1 (the "License"); you may not use this file
# * except in compliance with the License. You may obtain a copy of
# * the License at http://www.mozilla.org/MPL/
# *
# * Software distributed under the License is distributed on an "AS
# * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# * implied. See the License for the specific language governing
# * rights and limitations under the License.
# *
# * The Original Code is mozilla.org code.
# *
# * The Initial Developer of the Original Code is Sun Microsystems,
# * Inc. Portions created by Sun are
# * Copyright (C) 1999 Sun Microsystems, Inc. All
# * Rights Reserved.
# *
# * Contributor(s):
# */


$LogPath = "<HTML_ROOT_DIR>/log";
$URLRoot = "<HTML_ROOT>/log";
$LST_FILE = "<TEMP_LST_FILE>";
$RESULT_FILE = "<RESULT_FILE>";
$CURRENT_LOG = "<CURRENT_LOG_DIR>";
$DESCR_FILE = "$LogPath/<CURRENT_LOG_DIR>/<DESCRIPTION_FILE>";
$DESCRIPTION_FILE_NAME = "<DESCRIPTION_FILE>";
$COMPARISIONS_FOLDER_NAME = "<COMPARISIONS_FOLDER_NAME>";

$LogNPath = "";
$file_name = "";

$query = new CGI;

######################### Main ########################################

$stage = $query->param('stage');
if($stage == 1){
    CreateLogsList();
}elsif($stage == 2){
    $gold_res = $query->param('gold');
    $comp_res = $query->param('compared');
    #@logsToDiff = $query->param('LogsToDiff');
    DiffIt();
}
exit 0;

#######################################################################
sub CreateLogsList{
    opendir THATDIR, $LogPath;
    @allfiles = grep -d, map "$LogPath/$_", grep !/^\.\.?$/, readdir THATDIR;
    closedir THATDIR;
    $size = @allfiles;
    %IDs = ();
    for($i=0; $i<$size; $i++){
        $_ = $allfiles[$i];
        s#$LogPath/##;
        if($_=~/CurrentLog/){
        }else{
           if((-e "$LogPath/$_/$RESULT_FILE") && (-e "$LogPath/$_/$DESCRIPTION_FILE_NAME")){
               open(DESCR_FILE, "$LogPath/$_/$DESCRIPTION_FILE_NAME");
               @LINES = <DESCR_FILE>;
               close(DESCR_FILE);
               ($tmp, $descr) = split(/=/, $LINES[1]);
               ($tmp, $time) = split(/=/, $LINES[2]);
               chomp($descr);
               chomp($time);
               $descr = cutString($descr);
               $tmp = "Unique ID: $_ \\nExecution date and time: \\n$time \\nDescription: \\n$descr";
               $IDs{$_} = $tmp;
               #print "<tr><td><input type=checkbox name=TestsToDiff value=\"$_\">$_</td><td>$time</td><td>$descr</td></tr>\n";
           }else{
               #print "no file in : $_\n";
           }
        }
    }
    $javascript0  ="    function checkit() {\n        if(document.form1.gold.selectedIndex==document.form1.compared.selectedIndex) {\n";
    $javascript0 .="            alert(\"You cannot diff equals logs\\n\"+\n                  \"Please select a different logs to diff.\")\n";
    $javascript0 .="        }else{\n            document.form1.submit();\n        }\n";
    $javascript0 .="    }\n";
    $javascript1 .="    function Change_comment1() {\n"; 
    $javascript2 .="    function Change_comment2() {\n";
    $i=0;
    foreach $key(keys(%IDs)){
        $javascript1 .="        if (document.form1.gold.selectedIndex==$i) document.form1.first.value=\"$IDs{$key}\";\n";
        $javascript2 .="        if (document.form1.compared.selectedIndex==$i) document.form1.second.value=\"$IDs{$key}\";\n";
        $i++;
        $options_string1 .= "\n      <OPTION value=\"$key\" >$key</OPTION>";
        $options_string2 .= "\n      <OPTION value=\"$key\" >$key</OPTION>";
    }
    $javascript1 .= "    }\n";
    $javascript2 .= "    }\n";
    $javascript = "<script language=\"JavaScript\">\n".$javascript0.$javascript1.$javascript2."</script>\n";
    print "Content-type:text/html\n\n";
    print "<html>\n<head>\n<title>Select Log of responsiveness test for making diff</title>\n</head>\n";
    print "<body onload=\"Change_comment2();Change_comment1()\">\n$javascript\n<center><H1>Select Log of responsiveness test for making diff</H1></center>\n";
    print "<form action=\"<CGI_BIN_ROOT>/manager.cgi\" name=form1 method=post>\n<table border=0>\n";
    print "<tr>\n    <td width=\"30%\" align=right><b>Gold:</b> <SELECT name=gold onchange=\"Change_comment1()\">";
    print "$options_string1</SELECT>\n    </td>\n    <td><textarea name=first readonly rows=5 cols=40></textarea></td>\n</tr>\n";
    print "<tr>\n    <td align=right><b>Compared:</b> <SELECT name=compared onchange=\"Change_comment2()\">$options_string2</SELECT>";
    print "</td>\n    <td ><textarea name=second readonly rows=5 cols=40></textarea></td>\n</tr>\n";
    print "</table>\n";
    print "<input type=hidden name=stage value=2>\n<!input type=submit value=\"Make diff!\">\n";
    print "<input type=button name=but1 value=\"Make Diff!\" onclick=\"checkit()\">";
    print "</form>\n";
    print "</body>\n</html>";

}

#######################################################################
sub DiffIt{
    open(RESULT_FILE_GOLD, "$LogPath/$gold_res/$RESULT_FILE");
    @LINES_GOLD = <RESULT_FILE_GOLD>;
    close(RESULT_FILE_GOLD);
    open(RESULT_FILE_COMP, "$LogPath/$comp_res/$RESULT_FILE");
    @LINES_COMP = <RESULT_FILE_COMP>;
    close(RESULT_FILE_COMP);
    $SIZE_GOLD = @LINES_GOLD;
    $SIZE_COMP = @LINES_COMP;
    open(DESCRIPTION_FILE_GOLD, "$LogPath/$gold_res/$DESCRIPTION_FILE_NAME"); 
    @LINES_DESCR_GOLD = <DESCRIPTION_FILE_GOLD>;
    close(DESCRIPTION_FILE_GOLD);
    open(DESCRIPTION_FILE_COMP, "$LogPath/$comp_res/$DESCRIPTION_FILE_NAME"); 
    @LINES_DESCR_COMP = <DESCRIPTION_FILE_COMP>;
    close(DESCRIPTION_FILE_COMP);
    ($tmp, $descr_gold) = split(/=/, $LINES_DESCR_GOLD[1]);
    ($tmp, $time_gold) = split(/=/, $LINES_DESCR_GOLD[2]);
    chomp($descr_gold);
    chomp($time_gold);
    ($tmp, $descr_comp) = split(/=/, $LINES_DESCR_COMP[1]);
    ($tmp, $time_comp) = split(/=/, $LINES_DESCR_COMP[2]);
    chomp($descr_comp);
    chomp($time_comp);
    for($i=0;$i<$SIZE_GOLD;$i++){
        ($test_id, $result) =  split(/=/, $LINES_GOLD[$i]);
        chomp($result);
        $TestRes{$test_id} = $result;
    }
    for($i=0;$i<$SIZE_COMP;$i++){
        ($test_id, $result) =  split(/=/, $LINES_COMP[$i]);
        chomp($result);
        if(defined $TestRes{$test_id}){
            $tmp = $TestRes{$test_id};
            $TestRes{$test_id} = "$tmp".";"."$result";
        }else{
            $TestRes{$test_id} = "0;"."$result";
        }
    }
    foreach $key(keys(%TestRes)){
         if($TestRes{$key}=~/;/){}else{
             $TestRes{$key} = $TestRes{$key}.";0";
         }
    }
    $file_name = "$gold_res"."_"."$comp_res".".html";
    if(-d "$LogPath/$COMPARISIONS_FOLDER_NAME"){
        open(FILE_COMPARISION, ">$LogPath/$COMPARISIONS_FOLDER_NAME/$file_name");
    }else{
        mkdir("$LogPath/$COMPARISIONS_FOLDER_NAME", 0777) || die "cannot mkdir ";
        open(FILE_COMPARISION, ">$LogPath/$COMPARISIONS_FOLDER_NAME/$file_name");
    }
    print FILE_COMPARISION "<html>\n<head>\n<title>Diff of two tests</title>\n</head>\n";
    print FILE_COMPARISION "<body>\n<H1>Diff between two tests, UID\'s is: $gold_res & $comp_res</H1>\n";
    print FILE_COMPARISION "<font color=blue size=+1>Gold tests is: </font><br>\n<blockquote><b>Unique ID:</b> $gold_res<br>\n<b>Time:</b> $time_gold<br>\n<b>Description:</b> $descr_gold\n</blockquote>";
    print FILE_COMPARISION "<font color=red size=+1>Compared tests is: </font><br>\n<blockquote><b>Unique ID:</b> $comp_res<br>\n<b>Time:</b> $time_comp<br>\n<b>Description:</b> $descr_comp\n</blockquote>\n";
    print FILE_COMPARISION "<table border=1>\n";
    print FILE_COMPARISION "<tr>\n    <td align=center><b>Test name</b></td>\n    <td align=center><b>Gold Result</b></td>\n    <td align=center><b>Compared Result</b></td>\n    <td align=center><b> % </b></td>\n</tr>\n";
    foreach $key(keys(%TestRes)){
         ($gold, $comp) = split(/;/, $TestRes{$key});
         if($gold!=0 && $comp!=0){
             $proc = $gold - $comp / $gold;
             $proc = $proc * 100;
         }else{
             if($gold==0){
                 $gold = "<b>undef</b>";
             }
             if($comp==0){
                 $comp = "<b>undef</b>";
             }
             $proc = "<b>undef</b>";
         }
         print FILE_COMPARISION "<tr>\n    <td>$key</td>\n    <td align=center>$gold</td>\n    <td align=center>$comp</td>\n    <td align=center>";
         if($proc=~/undef/){
             print FILE_COMPARISION "$proc";
         }else{ 
             printf FILE_COMPARISION "%d", $proc;
         }
         print FILE_COMPARISION "</td>\n</tr>\n";
    }
    print FILE_COMPARISION "</table>\n";
    print FILE_COMPARISION "</body>\n</html>";
    close FILE_COMPARISION;
    redirect("$URLRoot/$COMPARISIONS_FOLDER_NAME/$file_name");
    
}

#######################################################################
sub cutString {
    $input = shift(@_);
    $in = 35;
    $output = "";
    $old_index = 0;
    $tmp = $input;
    while($in<length($input)) {
        $index = index($input, " ", $in);
        if($index!=-1){
            $output .= substr($tmp, 0, $index-$old_index)."\\n";
            $old_index = $index+1;
            $tmp = substr($input, $old_index, length($input));
            $in=$index+30;
        }else{
            $in+=30;
        }
    }
    return $output.$tmp;
}

#######################################################################
sub redirect {
    $new_url=shift(@_);
    print "Status: 301 Redirect\n";
    print "Content-type: text/html\n";
    print "Location: $new_url\n\n\n";
}
#######################################################################

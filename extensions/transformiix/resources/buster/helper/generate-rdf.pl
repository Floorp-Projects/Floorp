use File::Spec;
use File::Glob ':glob';

my(@chunks);
@list = ('conf');
go_in('conf', '', 'conf');
#exit 0;
print '<?xml version="1.0"?>

<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
         xmlns:nc="http://home.netscape.com/NC-rdf#">
  <rdf:Seq about="urn:root">
     <rdf:li><rdf:Description ID="conf"    nc:dir="conf" /></rdf:li>
     <rdf:li><rdf:Description ID="contrib" nc:dir="contrib" /></rdf:li>
     <rdf:li><rdf:Description ID="perf"    nc:dir="perf" /></rdf:li>
  </rdf:Seq>
';
print join('',@chunks);
print '</rdf:RDF>
';
exit 0;

sub go_in {
    my($current, $about, $cat) = @_;
    my (@list, $entry, @subdirs, @files, @purps, $rdf);
    chdir $current;
    @list = glob('*');
    
    LOOP: foreach $entry (@list) {
	next LOOP if $entry=~/^CVS$/;
	if (! -d $entry) {
	    if ($entry=~/^($current.*)\.xsl$/) {
		push(@files, $1);
		local ($purp);
		open STYLE, $entry;
		while (<STYLE>) {
		    if (/<!--\s+Purpose: (.+)\s*-->/) {
			$purp .= $1;
		    }
		}
		$purp=~s/"/'/g; $purp=~s/&/&amp;/g; $purp=~s/</&lt;/g;
		push(@purps, $purp);
	    }
	}
	else {
	    push(@subdirs, $entry);
	}
    }
    #print join(" ", @subdirs)."\n";
    my $topic = $about.$current; $topic=~s/\///g;
    $rdf = '<rdf:Seq about="#'.$topic."\" >\n";
    foreach $entry (@subdirs) {
	my $id = $about.$current.$entry; $id=~s/\///g;
	$rdf .= "<rdf:li><rdf:Description ID=\"$id\" nc:dir=\"$entry\" /></rdf:li>\n";
    }
    for (my $i=0; $i < @files; $i++) {
	my $uri = $about.$current.'/'.$files[$i];
	$uri=~s/[^\/]+\///;
	my $id = $uri; $id=~s/\///g;
	$rdf .= "<rdf:li><rdf:Description ID=\"$files[$i]\" nc:name=\"$files[$i]\" nc:purp=\"$purps[$i]\" nc:path=\"$uri\" nc:category=\"$cat\"/></rdf:li>\n";
    }
    $rdf .= "</rdf:Seq>\n";
    push(@chunks, $rdf);
    #print join(" ", @files)."\n";
    foreach $entry (@subdirs) {
	go_in($entry, $about.$current.'/', $cat);
    }
    chdir File::Spec->updir;
}

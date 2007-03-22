use File::Spec;

my(@chunks, @list, $entry, $main_cats, $spacing);
@list = ('conf', 'perf');
foreach $entry (@list) {
    $main_cats .= "     <rdf:li><rdf:Description about=\"urn:x-buster:$entry\" nc:dir=\"$entry\" /></rdf:li>\n";
    go_in($entry, '', $entry);
}
if ($ARGV[0]) {
    open OUTPUT, ">$ARGV[0]";
}
else {
    open OUTPUT, ">xalan.rdf";
};
select(OUTPUT);
print '<?xml version="1.0"?>

<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
         xmlns:nc="http://home.netscape.com/NC-rdf#">
  <rdf:Seq about="urn:root">
' . $main_cats . '  </rdf:Seq>
';
print join('',@chunks);
print '</rdf:RDF>
';
exit 0;

sub go_in {
    my($current, $about, $cat) = @_;
    my (@list, $entry, @subdirs, @files, @purps, $rdf);
    chdir $current;
    @list = <*>;
    
    LOOP: foreach $entry (@list) {
        next LOOP if $entry=~/^CVS$/;
        if (! -d $entry) {
            if ($entry=~/^($current.*)\.xsl$/) {
		local $source = $entry;
		$source=~s/xsl$/xml/;
		next LOOP if ! -f $source;
		$entry=~/^($current.*)\.xsl$/;
                push(@files, $1);
                local ($purp, $purp_open);
                open STYLE, $entry;
                $purp_open = 0;
                while (<STYLE>) {
                    if (/<!--\s+purpose: (.+)\s*-->/i) {
                        $purp .= $1;
                    }
                    elsif (/<!--\s+purpose: (.+)\s*$/i) {
                        $purp_open = 1;
                        $purp .= $1;
                    }
                    elsif ($purp_open) {
                        if (/\s*(\s.+)\s*-->/) {
                            $purp_open = 0;
                            $purp .= $1;
                        }
                        elsif (/\s*(\s.+)\s*$/) {
                            $purp .= $1;
                        }
                    }
                }
                $purp=~s/"/'/g; $purp=~s/&/&amp;/g; $purp=~s/</&lt;/g;
                $purp=~s/\r/ /g; $purp=~s/\s\s/ /g; $purp=~s/\s$//g;
                push(@purps, $purp);
            }
        }
        else {
            push(@subdirs, $entry);
        }
    }

    if (@subdirs > 0 || @files > 0) {
        my $topic = $about.$current; $topic=~s/\///g;
        $rdf = '  <rdf:Seq about="urn:x-buster:'.$topic."\">\n";
        foreach $entry (@subdirs) {
            if (go_in($entry, $about.$current.'/', $cat)) {
                my $id = 'urn:x-buster:'.$about.$current.$entry; $id=~s/\///g;
                $rdf .= "    <rdf:li><rdf:Description about=\"$id\" nc:dir=\"$entry\" /></rdf:li>\n";
            }
        }
        for (my $i=0; $i < @files; $i++) {
            my $uri = $about.$current.'/'.$files[$i];
            $uri=~s/[^\/]+\///;
            my $id = $uri; $id=~s/\///g;
            $rdf .= "    <rdf:li><rdf:Description about=\"urn:x-buster:$files[$i]\" nc:name=\"$files[$i]\" nc:purp=\"$purps[$i]\" nc:path=\"$uri\" nc:category=\"$cat\" /></rdf:li>\n";
        }
        $rdf .= "  </rdf:Seq>\n";
        push(@chunks, $rdf);
    }

    chdir File::Spec->updir;
    return (@subdirs > 0 || @files > 0);
}

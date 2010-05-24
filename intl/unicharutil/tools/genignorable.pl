#!/usr/bin/perl 

open $f, 'UnicodeData-Latest.txt' or die $!;
while (<$f>) {
  @columns = split(/;/);
#  print "$columns[0] : $columns[1]\n";
  $names{hex($columns[0])} = $columns[1];
}
close $f;

open $f, 'DerivedCoreProperties.txt' or die $!;
$re = '[';
while (<$f>) {
  next unless /Default_Ignorable_Code_Point/;
  next unless /^([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))?/;

  ($start, $end) = (hex($1), hex($2));
  $end = $start unless $end;

  for ($c = $start; $c <= $end; $c++) {
    printf "0x%04X", $c;
    printf " // $names{$c}" if $names{$c};
    print "\n";
  }

  if (!$prevend || $start > $prevend + 1) {
    $re .= make_unicode_range($prevstart, $prevend) if $prevstart;
    $prevstart = $start;
  }
  $prevend = $end;
}
$re .= make_unicode_range($prevstart, $prevend).']';
print STDERR $re;
close $f;

sub make_unicode_range
{
  my ($start, $end) = @_;

  if ($start > 0xffff) {
    my $starths = ($start - 0x10000) >> 10 | 0xd800;
    my $startls = ($start - 0x10000) & 0x3ff | 0xdc00;
    my $endhs = ($end - 0x10000) >> 10 | 0xd800;
    my $endls = ($end - 0x10000) & 0x3ff | 0xdc00;
    if ($starths == $endhs) {
      return sprintf("]|\\u%04x[\\u%04x-\\u%04x", $starths, $startls, $endls)
    }
    my $re = '';
    if ($startls > 0xdc00) {
      $re .= sprintf("]|\\u%04x[\\u%04x-\\udfff", $starths, $startls);
      $starths++;
    }
    if ($endhs > $starths) {
      $endhs-- if ($endls < 0xdfff);
      $re .= sprintf("]|[\\u%04x-\\u%04x][\\udc00-\\udfff", $starths, $endhs);
    }
    if ($endls < 0xdfff) {
      $re .= sprintf("]|\\u%04x[\\udc00-\\u%04x", $endhs, $endls);
    }
    return $re;
  } elsif ($start == $end) {
    return sprintf("\\u%04x", $start);
  } else {
    return sprintf("\\u%04x-\\u%04x", $start, $end);
  }
}

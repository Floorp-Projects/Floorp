#############################################################################
# $Id: LDIF.pm,v 1.7 1999/08/24 22:30:45 leif%netscape.com Exp $
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code was released March, 1999.  The Initial Developer of
# the Original Code is Netscape Communications Corporation.  Portions
# created by Netscape are Copyright (C) 1999 Netscape Communications
# Corporation.  All Rights Reserved.
#
# Contributor(s): Leif Hedstrom, John M. Kristian
#
#############################################################################

package Mozilla::LDAP::LDIF;

use vars qw($VERSION); $VERSION = "0.07";

require Exporter;
@ISA       = qw(Exporter);
@EXPORT    = qw();
@EXPORT_OK = qw(get_LDIF put_LDIF unpack_LDIF pack_LDIF set_Entry
	references next_attribute sort_attributes sort_entry
	delist_values enlist_values condense
	LDIF_get_DN get_DN
	read_v0 read_file_name read_v1 read_file_URL read_file_URL_or_name);

use strict;

BEGIN {
    eval 'use MIME::Base64';
    if ($@) {
	my $complaint = $@;
	eval q{
	    require Mozilla::LDAP::Utils;
	    *decode_base64 = \&Mozilla::LDAP::Utils::decodeBase64;
	    *encode_base64 = \&Mozilla::LDAP::Utils::encodeBase64;
	};
	if ($@) {
	    warn $complaint;
	    die "Can't use MIME::Base64";
# Get a copy from http://www.perl.com/CPAN-local/modules/by-module/MIME/
# and install it.  If you have trouble, try simply putting Base64.pm
# in a subdirectory named MIME, in one of the directories named in @INC
# (site_perl is a good choice).
	} elsif ($^W) {
	    warn $complaint;
	    warn "Can't use MIME::Base64";
	}
    }
}

sub _to_LDIF_records
    # Normalize a parameter list, to either an list of references to
    # arrays (and return 0) or a single record (and return 1).
    # Replace references to objects with the result of calling their
    # getLDIFrecords() method.
{
    my ($argv) = @_;
    use integer;
    my $i;
    for ($i = $[; $i <= $#$argv; ++$i) {
	my $type = ref $$argv[$i];
	if ($type) {
	    if ($type ne "ARRAY") {
		my @records = ($$argv[$i])->getLDIFrecords();
		splice @$argv, $i, 1, @records;
		$i += @records - 1;
	    }
	} elsif ($i == 0) {
	    return 1; # single record
	}
    }
    return 0; # zero or more references to records
}

sub _continue_lines
{
    my ($max_line, $from) = @_;
    # If $from contains '\n' bytes, they will be lost; that is, an LDIF
    # parser will not reconstruct them from the output.  But the remaining
    # characters are preserved, and the output is fairly legible, with an
    # LDIF continuation line break in place of each '\n' in $from.
    # This is useful for a person trying to read the value.

    my ($into) = "";
    foreach my $line (split /\n/, $from, -1) {
	$line = " $line" if length $into; # continuation of previous line
	if (defined $max_line) {
	    while ($max_line < length $line) {
		my $chunk;
		($chunk, $line) = unpack ("a${max_line}a*", $line);
		$into .= "$chunk\n";
		$line = " $line";
	    }
	}
	$into .= "$line\n";
    }
    return $into;
}

#############################################################################
# unpack/pack

sub unpack_LDIF
{
    my ($str, $read_ref, $option) = @_;
    $str =~ s"$/ ""g; # combine continuation lines
    $str =~ s"^#.*($/|$)""gm # ignore comments
	unless ((defined $option) and ("comments" eq lc $option));
    my (@record, $attr, $value);
    local $_;
    foreach $_ (split $/, $str) {
    	last if ($_ eq ""); # blank line
	if ($_ =~ /^#/) { # comment
	    ($attr, $value) = ($_, undef);
	} else {
	    ($attr, $value) = split /:/, $_, 2;
	    if (not defined ($value)) {
		warn "$0 non-LDIF line: $_\n" if ($^W and $attr ne "-");
	    } elsif ($value =~ s/^: *//) {
		$value = decode_base64 ($value);
	    } elsif ($value =~ s/^< *//) {
		my $temp = $value;
		$value = \$temp;
		if (defined $read_ref) { &$read_ref ($value); }
	    } else {
		$value =~ s/^ *//;
	    }
	}
	push @record, ($attr, $value);
    }
    return @record;
}

use vars qw($_std_encode); $_std_encode = '^[:< ]|[^ -\x7E]';

sub pack_LDIF
{
    my $max_line = shift;
    my $encode = undef;
    if ((ref $max_line) eq "ARRAY") {
	my @options = @$max_line; $max_line = undef;
	while (@options) {
	    my ($option, $value) = splice @options, 0, 2;
	    if      ("max_line" eq lc $option) { $max_line = $value;
	    } elsif ("encode"   eq lc $option) {
		$encode = ($value eq $_std_encode) ? undef : $value;
	    }
	}
    }
    $max_line = undef unless (defined ($max_line) and $max_line > 1);
    my $str = "";
    foreach my $record ((_to_LDIF_records \@_) ? \@_ : @_) {
	my @record = @$record;
	$str .= "\n" if length $str; # blank line between records
	while (@record) {
	    my ($attr, $val) = splice @record, 0, 2;
	    foreach $val (((ref $val) eq "ARRAY") ? @$val : $val) {
		if (not defined $val) {
		    $str .= _continue_lines ($max_line, $attr);
		} else {
		    my $value;
		    if (ref $val) {
			$value = "< $$val";
		    } elsif ($val eq "") {
			$value = ""; # output "$attr:"
		    } elsif ((defined $encode) ?
				$val =~ /$encode/ :
				$val =~ /$_std_encode/o) {
			$value = ": " . encode_base64 ($val, "");
		    } else {
			$value = " $val";
		    }
		    $str .= _continue_lines ($max_line, "$attr:$value");
		}
	    }
	}
    }
    return $str;
}

#############################################################################
# get/put

sub get_LDIF
{
    my ($fh, $eof, @options) = @_;
    $fh = *STDIN unless defined $fh;
    my (@record, $localEOF);

    $eof = (@_ > 1) ? \$_[$[+1] : \$localEOF;
    $$eof = "";
    do {
	my $str = "";
	my $line;
    	while (1) {
	    if (not defined ($line = <$fh>)) {
		$$eof = 1; last; # EOF from a file
	    }
	    $str .= $line;
	    if (not chomp $line) {
		$$eof = 1; last; # EOF from a terminal
	    } elsif ($line eq "") {
	        last;            # empty line
	    }
	}
	@record = unpack_LDIF ($str, @options);
    } until (@record or $$eof);
    return @record;
}

sub put_LDIF
{
    my $fh = shift;
    my $options = shift;
    $fh = select() unless defined $fh;
    foreach my $record ((_to_LDIF_records \@_) ? \@_ : @_) {
	no strict qw(refs); # $fh might be a string
	print $fh (pack_LDIF ($options, $record), "\n");
    }
}

#############################################################################
# object methods

sub new
{
    my ($class) = @_;
    my $self = {};

    if (@_ < 2) {
	$self->{"_fh_"} = *STDIN;
	$self->{"_rw_"} = "r";
    } else {
	$self->{"_fh_"} = $_[$[+1];
	if (@_ == 2) {
	    $self->{"_rw_"} = "rw";
	} else {
	    my $p2 = $_[$[+2];
	    my $p2type = ref $p2;
	    if ($p2type eq "CODE" or (@_ > 3 and not defined $p2)) {
		$self->{"_rw_"} = "r";
		$self->{"options"} = [$p2, $_[$[+3]];
	    } else {
		$self->{"_rw_"} = "w";
		$self->{"options"} = ($p2type eq "ARRAY") ? [@$p2] : $p2;
	    }
	}
	if (not $self->{"_fh_"}) {
	    if ($self->{"_rw_"} eq "w") {
		$self->{"_fh_"} = select(); # STDOUT, by default.
	    } else {
		$self->{"_fh_"} = *STDIN;
	    	$self->{"_rw_"} = "r";
	    }
	}
    }
    return bless $self, $class;
}

#############################################################################
# Destructor, close file descriptors etc. (???)
#
#sub DESTROY
#{
#  my $self = shift;
#}

sub get1
{
    my ($self) = @_;
    if ($self->{"_rw_"} ne "r") {
    	return unless ($self->{"_rw_"} eq "rw");
    	$self->{"_rw_"} = "r";
    }
    my $options = $self->{"options"};
    my $eof;
    my @record = get_LDIF ($self->{"_fh_"}, $eof,
			   defined $options ? @$options : ());
    if ($eof) { $self->{"_rw_"} = "eof"; }
    return @record;
}

sub get
{
    if (not ref $_[$[]) { # class method
        shift;
	return get_LDIF (@_);
    }
    use integer;
    if (@_ <= 1) {
	return get1 (@_);
    }
    my ($self, $num) = @_;
    my (@records, @record);
    $num = -1 unless defined $num;
    while (($num < 0 or $num-- != 0) and (@record = get1 ($self))) {
	push @records, [ @record ];
    }
    return @records;
}

sub put
{
    if (not ref $_[$[]) { # class method
        shift;
	return put_LDIF (@_);
    }
    my $self = shift;
    if ($self->{"_rw_"} ne "w") {
    	return unless ($self->{"_rw_"} eq "rw");
    	$self->{"_rw_"} = "w";
    }
    return put_LDIF ($self->{"_fh_"}, $self->{"options"}, @_);
}

#############################################################################
# Utilities

sub next_attribute
{
    my ($record, $offset) = @_;
    use integer;
    if (not defined $offset) { $offset = -2;
    } elsif ($offset % 2)  { --$offset; # make it even
    }
    my $i = $[ + $offset;
ATTRIBUTE:
    while (($i += 2) < $#$record) {
	my $value = \${$record}[$i+1];
	next unless defined $$value; # ignore comments and "-" lines
	my $option;
OPTION: for ($option = $[ + 2; $option < $#_; $option += 2) {
	    my ($keyword, $expression) = ((lc $_[$option]), $_[$option+1]);
	    my $exprType = ref $expression;
	    my $OK = 0;
	    if  ("name" eq $keyword or "type" eq $keyword) {
		next ATTRIBUTE unless defined $expression;
		next OPTION if ($exprType and ($exprType ne "CODE")); # unsupported
		foreach $_ (${$record}[$i]) {
		    last if ($OK = $exprType ? &$expression ($_) : eval $expression);
		}
	    } elsif ("value" eq $keyword) {
		next ATTRIBUTE unless defined $expression;
		next OPTION if ($exprType and ($exprType ne "CODE")); # unsupported
		foreach $_ (((ref $$value) eq "ARRAY") ? @$$value : $$value) {
		    last if ($OK = $exprType ? &$expression ($_) : eval $expression);
		}
	    } else { # unsupported keyword
		last OPTION;
	    }
	    next ATTRIBUTE unless $OK;
	}
	return $i - $[;
    }
    return undef;
}

sub references
{
    my @refs;
    use integer;
    foreach my $record ((_to_LDIF_records \@_) ? \@_ : @_) {
	my $i = undef;
	while (defined ($i = next_attribute ($record, $i))) {
	    my $vref = \${$record}[$[+$i+1];
	    my $vtype = ref $$vref;
	    if ($vtype eq "ARRAY") { # a list
		foreach my $value (@$$vref) {
		    if (ref $value) {
			push @refs, \$value;
		    }
		}
	    } elsif ($vtype) {
		push @refs, $vref;
	    }
	}
    }
    return @refs;
}

sub _carpf
{
    my $msg = sprintf shift, @_;
    require Carp;
    Carp::carp $msg;
}

use vars qw($_uselessUseOf);
$_uselessUseOf = "Useless use of ".__PACKAGE__."::%s in scalar or void context";

sub enlist_values
{
    use integer;
    my $single = _to_LDIF_records \@_;
    if ($^W and not $single and @_ > 1 and not wantarray) {
	_carpf ($_uselessUseOf, "enlist_values");
    }
    my @results;
    foreach my $record ($single ? \@_ : @_) {
	my ($i, @result, %first, $isEntry);
	for ($i = $[+1; $i <= $#$record; $i += 2) {
	    my ($attr, $value) = (${$record}[$i-1], ${$record}[$i]);
	    if (not defined $value) {
		%first = () # Don't enlist values separated by a "-" line.
			 unless ($attr =~ /^#/); # but comments don't matter.
		push @result, ($attr, $value);
	    } else {
		if (not defined $isEntry) { # Decide whether this is an entry:
		    $isEntry = (lc ${$record}[$[]) eq "dn";
		    if ($isEntry) {
			$isEntry = (lc ${$record}[$[+2]) ne "changetype";
			$isEntry = (lc ${$record}[$[+3]) eq "add" unless $isEntry;
		    } # A single Boolean expression would be better, except it makes
		      # SunOS Perl 5 carp "Useless use of string ne in void context".
		}
		my $firstValue = $first{lc $attr};
		if (not defined $firstValue) {
		    $firstValue = [];
		    # Enlist non-adjacent values only in an entry.
		    %first = () unless $isEntry;
		    $first{lc $attr} = $firstValue;
		    push @result, ($attr, $firstValue);
		}
		push @$firstValue, ((ref $value) eq "ARRAY") ? @$value : $value;
	    }
	}
	for ($i = $[+1; $i <= $#result; $i += 2) {
	    next unless defined $result[$i];
	    my $len = scalar @{$result[$i]};
	    if      ($len == 0) { $result[$i] = undef; # how did this happen?
	    } elsif ($len == 1) { $result[$i] = ${$result[$i]}[$[];
	    }
	}
	foreach my $r (references (\@result)) { # don't return the same reference
	    my $v = $$$r; $$r = \$v; # return a reference to a copy of the scalar
	}
	push @results, \@result;
    }
    return $single ? @{$results[$[]} : @results;
}

sub condense
{
    _carpf ("Use ".__PACKAGE__."::enlist_values"
	   ." (instead of obsolescent condense)") if $^W;
    # Unlike condense, enlist_values does not modify the records
    # you pass to it; it returns newly created, equivalent records.
    foreach my $record (@_) {
	@$record = @{(enlist_values ($record))[$[]};
    }
}

sub delist_values
{
    use integer;
    my $single = _to_LDIF_records \@_;
    if ($^W and not $single and @_ > 1 and not wantarray) {
	_carpf ($_uselessUseOf, "delist_values");
    }
    my @results;
    foreach my $record ($single ? \@_ : @_) {
	my ($i, @result);
	for ($i = $[+1; $i <= $#$record; $i += 2) {
	    my ($attr, $value) = (${$record}[$i-1], ${$record}[$i]);
	    foreach $value (((ref $value) eq "ARRAY") ? @$value : $value) {
		push @result, ($attr, $value);
	    }
	}
	if ($i == $#$record + 1) { # weird.  Well, don't lose it:
	    push @result, ${$record}[$i-1];
	}
	foreach my $r (references (\@result)) { # don't return the same reference
	    my $v = $$$r; $$r = \$v; # return a reference to a copy of the scalar
	}
	push @results, \@result;
    }
    return $single ? @{$results[$[]} : @results;
}

sub _k
{
    my $val = shift;
    return (ref $val) ? "<$$val" : " $val"; # references come last
}

sub _byAttrValue
{
    ((lc $a->[ $[ ]) cmp (lc $b->[ $[ ])) # ignore case in attribute names
 or ((_k $a->[$[+1]) cmp (_k $b->[$[+1]));
}

sub _shiftAttr
    # Given a reference to an LDIF record, remove the first two elements
    # (usually an attribute type and value) and also any subsequent
    # non-attributes (comments, "-" lines or non-LDIF lines).
    # Return a reference to an array containing the removed values.
{
    my ($from) = @_;
    my $next = next_attribute ($from, 0);
    return [ splice @$from, 0, $next ] if defined $next;
    my @into = splice @$from, 0;
    push @into, (undef) if (@into % 2);
    return \@into;
}

sub sort_attributes
    # Comments, "-" lines and non-LDIF lines are sorted with the attribute that
    # immediately precedes them.
{
    use integer;
    my $single = _to_LDIF_records \@_;
    if ($^W and not $single and @_ > 1 and not wantarray) {
	_carpf ($_uselessUseOf, "sort_attributes");
    }
    my (@results, @result, @preamble);
    foreach my $record ($single ? \@_ : @_) {
	@result = @{(delist_values ($record))[$[]};
	@preamble = ();
	if (@result > 1 and not defined $result[$[+1]) { # initial comments
	    push @preamble, @{_shiftAttr \@result};
	}
	if (@result and ("dn" eq lc $result[$[])) {
	    push @preamble, @{_shiftAttr \@result}; # dn => "cn=etc..."
	    if ("changetype" eq lc $result[$[]) { # this is a change, not an entry.
		if ("add" eq lc $result[$[+1]) {
		    push @preamble, @{_shiftAttr \@result}; # changetype => "add"
		} else { # It's possible to sort this, but it doesn't seem useful.
		    next; # So just return it, unmodified.
		}
	    }
	}

	my @pairs = ();
	while (@result >= 2) {
	    push @pairs, (_shiftAttr \@result);
	}
	@pairs = sort _byAttrValue @pairs;

	my $pair;
	while ($pair = pop @pairs) {
	    unshift @result, (@{$pair});
	}
    } continue {
	unshift @result, @preamble;
	push @results, \@result;
    }
    return $single ? @{$results[$[]} : @results;
}
*sort_entry = \&sort_attributes; # for compatibility with prior versions.

sub get_DN
{
    use integer;
    my $single = _to_LDIF_records \@_;
    if ($^W and not $single and @_ > 1 and not wantarray) {
	_carpf ($_uselessUseOf, "get_DN");
    }
    my @DNs;
    foreach my $record ($single ? \@_ : @_) {
	my $i = next_attribute ($record);
	push @DNs, (((defined $i) and ("dn" eq lc $record->[$[+$i])) ?
		$record->[$[+$i+1] : undef);
    }
    return $single ? $DNs[$[] : @DNs;
}
*LDIF_get_DN = \&get_DN;

sub read_file_name
{
    my $name = ${$_[$[]};
    local *FILE;
    if (open (FILE, "<$name")) {
	binmode FILE;
	$_[$[] = "";
	while (read (FILE, $_[$[], 1024, length ($_[$[]))) {}
	close FILE;
	return 1;
    }
    warn "$0 can't open $name: $!\n" if $^W;
    return ""; # failed
}

sub read_file_URL
{
    my $URL = ${$_[$[]};
    if ($URL =~ s/^file://i) {
	$URL =~ s'^///'/'; # file:///x == file:/x
	my $value = \$URL;
	if (read_file_name ($value)) {
	    $_[$[] = $value;
	    return 1;
	}
    }
    return ""; # failed
}

*read_v0 = \&read_file_name;
*read_v1 = \&read_file_URL;

sub read_file_URL_or_name
{
    read_file_URL (@_) or read_file_name (@_);
}

#############################################################################
# Mozilla::LDAP::Entry support

sub set_Entry
{
    my ($entry, $record) = @_;
    return unless defined $record;
    ($record) = enlist_values ($record);
    foreach my $r (references ($record)) {
	read_file_URL_or_name ($$r);
    }
    my $skip;
    while (defined ($skip = next_attribute ($record))) {
	if ($skip) { splice @$record, 0, $skip; }
	my ($attr, $value) = splice @$record, 0, 2;
	if ("dn" eq lc $attr) { $entry->setDN ($value);
	} else { $entry->{$attr} = ((ref $value) eq "ARRAY") ? $value : [$value];
	}
    }
    return $entry;
}

use vars qw($_used_Entry); $_used_Entry = "";

sub _use_Entry
{
    return if $_used_Entry; $_used_Entry = 1;
    eval 'use Mozilla::LDAP::Entry()';
    if ($@) {
	warn $@;
	# But don't die.  Perhaps we're using another, compatible class.
    } elsif (not can Mozilla::LDAP::Entry 'getLDIFrecords') {
	# 'eval' prevents 'sub' from happening at compile time.
        eval q{
	    package Mozilla::LDAP::Entry;
	    sub getLDIFrecords
	    {
		my ($self) = @_;
		my @record = (dn => $self->getDN());
		# The following depends on private components of LDAP::Entry.
		# This is bad.  But the public interface wasn't sufficient.
		foreach my $attr (@{$self->{"_oc_order_"}}) {
		    next if ($attr =~ /^_.+_$/);
		    next if $self->{"_${attr}_deleted_"};
		    push @record, ($attr => $self->{$attr});
		    # This is dangerous: @record and %$self now both contain
		    # references to the same array.  To avoid this, copy it:
		    # push @record, ($attr => [@{$self->{$attr}}]);
		    # But that's not necessary, because the array and its
		    # contents are not modified as a side-effect of getting
		    # other attributes, from this or other LDAP::Entry's.
		}
		return \@record;
	    }
	}; # eval
    }
}

#############################################################################
# Read multiple entries, and return an array of objects.
# The first parameter is the number to read (default: read the entire file).
#
sub readEntries
{
    my ($self, $num, $factory) = @_;
    # This function could be extended, to enable the caller to supply
    # a factory class name or object reference as the second parameter.
    my @records = $self->get ($num);
    require Mozilla::LDAP::Conn if @records; # and not defined $factory
    my ($record, @entries);
    while ($record = shift @records) {
	push @entries, set_Entry ((#(defined $factory) ? $factory->newEntry() :
				   Mozilla::LDAP::Conn->newEntry()),
				  $record);
    }
    return @entries;
}

#############################################################################
# Read the next $entry from an ::LDIF object.
#
sub readOneEntry
{
    my $self = shift;
    my ($entry) = $self->readEntries (1, @_);
    return $entry;
}
*readEntry = \&readOneEntry;

#############################################################################
# Write multiple entries, the argument is the array of Entry objects.
#
sub writeEntries
{
    _use_Entry();
    return put (@_);
}

#############################################################################
# Print one entry, to the file handle.
#
sub writeOneEntry
{
    my ($self, $entry) = @_; # ignore other parameters
    return $self->writeEntries ($entry);
}
*writeEntry = \&writeOneEntry;


#############################################################################
# Mandatory TRUE return value.
#
1;


#############################################################################
# POD documentation...
#
__END__

=head1 NAME

Mozilla::LDAP::LDIF - read or write LDIF (LDAP Data Interchange Format)

=head1 SYNOPSIS

 use Mozilla::LDAP::LDIF
    qw(set_Entry get_LDIF put_LDIF unpack_LDIF pack_LDIF
       sort_attributes references enlist_values delist_values
       read_v1 read_v0 read_file_URL_or_name);

 $ldif = new Mozilla::LDAP::LDIF (*FILEHANDLE, \&read_reference, $comments);
 @record = get $ldif;
 @records = get $ldif ($maximum_number);
 $entry = set_Entry (\entry, \@record);
 $entry = readOneEntry $ldif;
 @entries = readEntries $ldif ($maximum_number);

 $ldif = new Mozilla::LDAP::LDIF (*FILEHANDLE, $options);
 $success = put $ldif (@record);
 $success = put $ldif (\@record, \object ...);
 $success = writeOneEntry $ldif (\entry);
 $success = writeEntries  $ldif (\entry, \entry ...);

 @record = get_LDIF (*FILEHANDLE, $eof, \&read_reference, $comments);
 @record = get_LDIF; # *STDIN

 $success = put_LDIF (*FILEHANDLE, $options, @record);
 $success = put_LDIF (*FILEHANDLE, $options, \@record, \object ...);

 @record = unpack_LDIF ($string, \&read_reference, $comments);

 $string = pack_LDIF ($options, @record);
 $string = pack_LDIF ($options, \@record, \object ...);

 @record = enlist_values (@record);
 @record = delist_values (@record);

 @record = sort_attributes (@record);

 $DN  = LDIF_get_DN (@record); # alias get_DN
 @DNS = LDIF_get_DN (\@record, \object ...); # alias get_DN

 $offset = next_attribute (\@record, $offset, @options);

 @references = references (@record);
 @references = references (\@record, \object ...);

 $success = read_v1 (\$url);  # alias read_file_URL
 $success = read_v0 (\$name); # alias read_file_name
 $success = read_file_URL_or_name (\$url_or_name);

=head1 REQUIRES

MIME::Base64 (or Mozilla::LDAP::Utils), Exporter, Carp

=head1 INSTALLATION

Put the LDIF.pm file into a subdirectory named Mozilla/LDAP,
in one of the directories named in @INC.
site_perl is a good choice.

=head1 EXPORTS

Nothing (unless you request it).

=head1 DESCRIPTION

LDIF version 1 is defined by F<E<lt>draft-good-ldap-ldif-03E<gt>>.
An LDIF record like this:

    DN: cn=Foo Bar, o=ITU
    cn: Foo Bar
    Sn: Bar
    objectClass: person
    objectClass: organizatio
     nalPerson
    jpegPhoto:< file:foobar.jpg
    # comment

corresponds (in this module) to a Perl array like this:

    (DN => "cn=Foo Bar, o=ITU",
     cn => "Foo Bar",
     Sn => "Bar",
     objectClass => [ "person", "organizationalPerson" ],
     jpegPhoto => \"file:foobar.jpg",
     '# comment', undef
    )

URLs or file names are read by a separate function.
This module provides functions to read a file name (LDIF version 0)
or a file URL that names a local file (minimal LDIF version 1), or either.
You can supply a similar function to read other forms of URL.

Most output and utility methods in this module accept a parameter list
that is either an LDIF array (the first item is a string, usually "dn"),
or a list of references, with each reference pointing to either
an LDIF array or an object from which this module can get LDIF arrays
by calling the object's B<getLDIFrecords> method.
This module calls $object->getLDIFrecords(), expecting it to
return a list of references to LDIF arrays.
getLDIFrecords may return references to the object's own data,
although it should not return references to anything that will be
modified as a side-effect of another call to getLDIFrecords(),
on any object.

=head1 METHODS

=head2 Input

=over 4

=item B<new> Mozilla::LDAP::LDIF (*FILEHANDLE, \&read_reference, $comments)

Create and return an object to read LDIF from the given file.
If *FILEHANDLE is not defined, return an object to read from *STDIN.

If \&read_reference is defined, call it when reading each reference
to another data source, with ${$_[$[]} equal to the reference.
The function should copy the referent (for example, the contents of
the named file) into $_[$[].

Ignore LDIF comment lines, unless $comments eq "comments".

=item B<get> $ldif

Read an LDIF record from the given file.
Combine continuation lines and base64-decode attribute values.
Return an array of strings, representing the record.  Return a
false value if end of file is encountered before an LDIF record.

=item B<get> $ldif ($maximum_number)

Read LDIF records from the given file, until end of file is encountered
or the given $maximum_number of records are read.
If $maximum_number is undef (or negative), read until end of file.
Return an array of references to arrays, each representing one record.
Return a false value if end of file is encountered before an LDIF record,
or $maximum_number is zero.

=item B<readOneEntry> $ldif

=item B<readEntries> $ldif ($maximum_number)

Read Mozilla::LDAP::Entry objects from the given file, and
return references to them.
Call Mozilla::LDAP::Conn->newEntry() to create each returned object.
Return a false value if end of file is encountered before an LDIF record,
or $maximum_number is zero.
B<readOneEntry> returns a reference to a single object.
B<readEntries> returns an array of references to as many as $maximum_number
objects.
See B<get> (above) for more information.

=item B<set_Entry> (\entry, \@record)

Set the DN and attributes of the given Mozilla::LDAP::Entry object
from the given LDIF record.  Return a reference to the entry.

=item B<get_LDIF> (*FILEHANDLE, $eof, \&read_reference, $comments)

Read an LDIF record from the given file.
Return an array of strings, representing the record.  Return a
false value if end of file is encountered before an LDIF record.

If *FILEHANDLE is not defined, read from *STDIN.

If $eof is passed, set it true if the end of the given file was
encountered; otherwise set it false.
This function may set $eof false and also return a record
(if the record was terminated by the end of file).

If \&read_reference is defined, call it when reading each reference
to another data source, with ${$_[$[]} equal to the reference.
The function should copy the referent (for example, the contents of
the named file) into $_[$[].

Ignore LDIF comment lines, unless $comments eq "comments".

=item B<unpack_LDIF> ($string, \&read_reference, $comments)

Read one LDIF record from the given string.
Return an array of strings, representing the record.  Return a
false value if the given string doesn't contain an LDIF record.

If \&read_reference is defined, call it when reading each reference
to another data source, with ${$_[$[]} equal to the reference.
The function should copy the referent (for example, the contents of
the named file) into $_[$[].

Ignore LDIF comment lines, unless $comments eq "comments".

=item B<read_v1> (\$url)

=item B<read_file_URL> (\$url)

Change the parameter, from a reference to a URL (string) to a string containing
a copy of the contents of the file named by that URL, and return true.
Return false if the URL doesn't name a local file, or the file can't be read.

This implements LDIF version 1, although it doesn't support URLs that refer
to anything but a local file (e.g. HTTP or FTP URLs).

=item B<read_v0> (\$name)

=item B<read_file_name> (\$name)

Change the parameter, from a reference to a file name to a string containing
a copy of the contents of that file, and return true.
Return false if the file can't be read.

This implements LDIF version 0.

=item B<read_file_URL_or_name> (\$url_or_name)

Change the parameter, from a reference to a URL or file name, to a string
containing a copy of the contents of the file it names, and return true.
Return false if the file can't be read.

=back

=head2 Output

=over 4

=item B<new> Mozilla::LDAP::LDIF (*FILEHANDLE, $options)

Create and return an object used to write LDIF to the given file.
$options are described below.

=item B<put> $ldif (@record)

=item B<put> $ldif (\@record, \object ...)

=item B<put_LDIF> (*FILEHANDLE, $options, @record)

=item B<put_LDIF> (*FILEHANDLE, $options, \@record, \object ...)

Write LDIF records to the given file.
$options are described below.

=item B<writeOneEntry> $ldif (\entry)

=item B<writeEntries>  $ldif (\entry, \entry ...)

Write Mozilla::LDAP::Entry objects to the given file.

=item B<pack_LDIF> ($options, @record)

=item B<pack_LDIF> ($options, \@record, \object ...)

Return an LDIF string, representing the given records.

=item B<$options>

The options parameter (above) may be either
C<undef>, indicating all default options, or
a number, which is equivalent to C<[max_line =E<gt>>I< number>C<]>, or
a reference to an array that contains a list of options, composed from:

=over 4

=item C<max_line =E<gt>>I< number>

If I<number> > 1, break output into continuation lines, so no line
is longer than I<number> bytes (not counting the end-of-line marker).

Default: 0 (output is not broken into continuation lines).

=item C<encode =E<gt>>I< pattern>

Base64 encode output values that match I<pattern>.
Warning: As a rule, your I<pattern> should match any value that contains '\n'.
If any such value is not Base64 encoded, it will be output in a form
that does not represent the '\n' bytes in LDIF form.
That is, if the output is parsed as LDIF, the resulting value will be
like the original value, except the '\n' bytes will be removed.

Default: C<"^[:E<lt> ]|[^ -\x7E]">

=back

For example:

    pack_LDIF ([encode=>"^ |[^ -\xFD]"], @record)

returns a string in which UTF-8 strings are not encoded
(unless they begin with a space or contain control characters)
and lines are not continued.
Such a string may be easier to view or edit than standard LDIF,
although it's more prone to be garbled when sent in email
or processed by software designed for ASCII.
It can be parsed without loss of information (by unpack_LDIF).

=back

=head2 Utilities

=over 4

=item B<sort_attributes> (@record)

=item B<sort_attributes> (\@record, \object ...)

Return a record equivalent to each parameter, except with the attributes
sorted, primarily by attribute name (ignoring case) and secondarily by
attribute value (using &cmp).
If the parameter list is a single record, return a single record;
otherwise return a list of references to records.

=item B<enlist_values> (@record)

=item B<enlist_values> (\@record, \object ...)

Return a record equivalent to the parameter, except with values of
the same attribute type combined into a nested array.  For example,

    enlist_values (givenName => "Joe", givenname => "Joey", GivenName => "Joseph")

returns

    (givenName => ["Joe", "Joey", "Joseph"])

If the parameter list is a single record, return a single record;
otherwise return a list of references to records.

=item B<delist_values> (@record)

=item B<delist_values> (\@record, \object ...)

Return a record equivalent to the parameter, except with all values
contained directly (not in a nested array).  For example,

    delist_values (givenName => ["Joe", "Joey", "Joseph"])

returns

    (givenName => "Joe", givenName => "Joey", givenName => "Joseph")

If the parameter list is a single record, return a single record;
otherwise return a list of references to records.

=item B<references> (@record)

=item B<references> (\@record, \object ...)

In list context, return a list of references to each of the references
to external data sources, in the given records.
In scalar context, return the length of that list; that is, the total
number of references to external data sources.

=item LDIF_get_DN (@record)

=item      get_DN (@record)

Return the DN of the given record.
Return undef if the first attribute of the record isn't a DN.

=item LDIF_get_DN (\@record, \object ...)

=item      get_DN (\@record, \object ...)

Return the DN of each of the given records,
as an array with one element for each parameter.
If a given record's first attribute isn't a DN,
the corresponding element of the returned array is undef.

=item next_attribute (\@record, $offset, @options)

Return the offset of an attribute type in the given record.
Search forward, starting at $offset + 1, or 0 if $offset is not defined.
Return undef if no attribute is found.
The @options list is composed of zero or more of the following:

=over 4

=item C<name =E<gt> >I<expression>

=item C<type =E<gt> >I<expression>

Don't return an offset unless the given I<expression> evaluates to TRUE,
with $_ aliased to the attribute type name.

=item C<value =E<gt> >I<expression>

Don't return an offset unless the given I<expression> evaluates to TRUE,
with $_ aliased to one of the attribute values.

=back

In either case, the I<expression> may be a string, which is simply
evaluated (using B<eval>), or
a reference to a subroutine, which is called with $_ as its only parameter.
The value returned by B<eval> or the subroutine is taken as the
result of evaluation.

If no options are given, the offset of the next attribute is returned.

Option expressions can modify the record,
since they are passed an alias to an element of the record.
An option can selectively prevent the evaluation of subsequent options:
options are evaluated in the order they appear in the @options list, and
if an option evaluates to FALSE, subsequent options are not evaluated.

=back

=head1 DIAGNOSTICS

=over 4

=item $0 can't open %s: $!

(W) Mozilla::LDAP::LDIF::read_file_* failed to open a file,
probably named in an LDIF attrval-spec.

=item $0 non-LDIF line: %s

(D) The input contains a line that can't be parsed as LDIF.
It is carried along in place of an attribute name, with an undefined value.
For example, B<unpack_LDIF>("abc") outputs this warning, and returns ("abc", undef).

=item Can't use MIME::Base64

(F) The MIME::Base64 module isn't installed, and
Mozilla::LDAP::Utils can't be used (as an inferior substitute).
To rectify this, get a copy of MIME::Base64 from
http://www.perl.com/CPAN-local/modules/by-module/MIME/ and install it.
If you have trouble, try simply putting Base64.pm in a subdirectory named MIME,
in one of the directories named in @INC (site_perl is a good choice).
You'll get a correct, but relatively slow implementation.

=item Useless use of %s in scalar or void context

(W) The function returns multiple records, of which all but the last
will be ignored by the caller.  Time and space were wasted to create them.
It would probably be better to call the function in list context, or
to pass it only a single record.

=back

=head1 EXAMPLES

    use Mozilla::LDAP::LDIF qw(read_file_URL_or_name);

    $in  = new Mozilla::LDAP::LDIF (*STDIN, \&read_file_URL_or_name);
    $out = new Mozilla::LDAP::LDIF (*STDOUT, 78);
    @records = get $in (undef); # read to end of file (^D)
    put $out (@records);

    use Mozilla::LDAP::Conn();

    $conn = new Mozilla::LDAP::Conn (...);
    while ($entry = readOneEntry $in) {
        add $conn ($entry);
    }

    use Mozilla::LDAP::LDIF qw(get_LDIF put_LDIF
        references read_v1 next_attribute sort_attributes);

    while (@record = get_LDIF (*STDIN, $eof)) {
        # Resolve all the file URLs:
        foreach my $r (references (@record)) {
            read_v1 ($$r);
        }
        # Capitalize all the attribute names:
        for ($r = undef; defined ($r = next_attribute (\@record, $r)); ) {
            $record[$r] = ucfirst $record[$r];
        }
        # Capitalize all the title values:
        next_attribute (\@record, undef,
                        type => '"title" eq lc $_',
                        value => '$_ = ucfirst; 0');
	# Sort the attributes and output the record, 78 characters per line:
        put_LDIF (*STDOUT, 78, sort_attributes (@record));
        last if $eof;
    }

=head1 AUTHOR

John Kristian <kristian@netscape.com>

Thanks to Leif Hedstrom, from whose code I took ideas.
But I accept all blame.

=head1 SEE ALSO

L<Mozilla::LDAP::Entry>, L<Mozilla::LDAP::Conn>, and of course L<Perl>.

=cut

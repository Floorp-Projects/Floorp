#!/usr/bin/perl -w
# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# Preprocessor
# Version 1.0
#
# Copyright (c) 2002, 2003 by Ian Hickson
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

# Thanks to bryner and bsmedberg for suggestions.

use strict;

# takes as arguments the files to process
# defaults to stdin
# output to stdout

my $stack = new stack;

# command line arguments
my @includes;
while ($_ = $ARGV[0], defined($_) && /^-./) {
    shift;
    last if /^--$/os;
    if (/^-D(.*)$/os) { 
        for ($1) {
            if (/^(\w+)=(.*)$/os) {
                $stack->define($1, $2);
            } elsif (/^(\w+)$/os) {
                $stack->define($1, 1);
            } else {
                die "$0: invalid argument to -D: $_\n";
            }
        }
    } elsif (/^-F(.*)$/os) { 
        for ($1) {
            if (/^(\w+)$/os) {
                $stack->filter($1, 1);
            } else {
                die "$0: invalid argument to -F: $_\n";
            }
        }
    } elsif (/^-I(.*)$/os) { 
        push(@includes, $1);
    } elsif (/^-E$/os) { 
        foreach (keys %ENV) {
            # define all variables that have valid names
            $stack->define($_, $ENV{$_}) unless m/\W/;
        }
    } elsif (/^-d$/os) { 
        $stack->{'dependencies'} = 1;
    } else {
        die "$0: invalid argument: $_\n";
    }
}
unshift(@ARGV, '-') unless @ARGV;
unshift(@ARGV, @includes);

# do the work
foreach (@ARGV) { include($stack, $_); }
exit(0);

########################################################################

package main;
use File::Spec 0.8;
use File::Spec::Unix; # on all platforms, because the #include syntax is unix-based

sub include {
    my($stack, $filename) = @_;
    my $directory = $stack->{'variables'}->{'DIRECTORY'};
    if ($filename ne '-') {
        $filename = File::Spec->rel2abs($filename, $directory);
        my($volume, $path) = File::Spec->splitpath($filename);
        $directory = File::Spec->catpath($volume, $path, '');
    }
    local $stack->{'variables'}->{'DIRECTORY'} = $directory;
    local $stack->{'variables'}->{'FILE'} = $filename;
    local $stack->{'variables'}->{'LINE'} = 0;
    local *FILE;
    open(FILE, $filename) or die "Couldn't open $filename: $!\n";
    while (<FILE>) {
        # on cygwin, line endings are screwed up, so normalise them.
        s/[\x0D\x0A]+$/\n/os if $^O eq 'cygwin';
        $stack->newline;
        if (/^\#([a-z]+)\n?$/os) { # argumentless processing instruction
            process($stack, $1);
        } elsif (/^\#([a-z]+)\s(.*?)\n?$/os) { # processing instruction with arguments
            process($stack, $1, $2);
        } elsif (/^\#\n?/os) { # comment
            # ignore it
        } elsif ($stack->enabled) {
            # print it, including any newlines
            $stack->print(filtered($stack, $_));
        }
    }
    close(FILE);
}

sub process {
    my($stack, $instruction, @arguments) = @_;
    my $method = 'preprocessor'->can($instruction);
    if (not defined($method)) {
        fatal($stack, 'unknown instruction', $instruction);
    }
    eval { &$method($stack, @arguments) };
    if ($@) {
        fatal($stack, "error evaluating $instruction:", $@);
    }
}

sub filtered {
    my($stack, $text) = @_;
    foreach my $filter (sort keys %{$stack->{'filters'}}) {
        next unless $stack->{'filters'}->{$filter};
        my $method = 'filter'->can($filter);
        if (not defined($method)) {
            fatal($stack, 'unknown filter', $filter);
        }
        $text = eval { &$method($stack, $text) };
        if ($@) {
            fatal($stack, "error using $filter:", $@);
        }                
    }
    return $text;
}

sub fatal {
    my $stack = shift;
    my $filename = $stack->{'variables'}->{'FILE'};
    local $" = ' ';
    print STDERR "$0:$filename:$.: @_\n";
    exit(1);
}


########################################################################

package stack;

sub new {
    return bless {
        'variables' => {
            # %ENV,
            'LINE' => 0, # the line number in the source file
            'DIRECTORY' => '', # current directory
            'FILE' => '', # source filename
            '1' => 1, # for convenience (the constant '1' is thus true)
        },
        'filters' => {
            # filters
        },
        'values' => [], # the value of the last condition evaluated at the nth lewel
        'lastPrinting' => [], # whether we were printing at the n-1th level
        'printing' => 1, # whether we are currently printing at the Nth level
        'dependencies' => 0, # whether we are showing dependencies
    };
}

sub newline {
    my $self = shift;
    $self->{'variables'}->{'LINE'}++;
}

sub define {
    my $self = shift;
    my($variable, $value) = @_;
    die "not a valid variable name: '$variable'\n" if $variable =~ m/\W/;
    $self->{'variables'}->{$variable} = $value;
}

sub defined {
    my $self = shift;
    my($variable) = @_;
    die "not a valid variable name: '$variable'\n" if $variable =~ m/\W/;
    return defined($self->{'variables'}->{$variable});
}

sub undefine {
    my $self = shift;
    my($variable) = @_;
    die "not a valid variable name: '$variable'\n" if $variable =~ m/\W/;
    delete($self->{'variables'}->{$variable});
}

sub get {
    my $self = shift;
    my($variable, $required) = @_;
    die "not a valid variable name: '$variable'\n" if $variable =~ m/\W/;
    my $value = $self->{'variables'}->{$variable};
    if (defined($value)) {
        return $value;
    } else {
        die "variable '$variable' is not defined\n" if $required;
        return '';
    }
}

sub push {
    my $self = shift;
    my($value) = @_;
    push(@{$self->{'values'}}, $value);
    push(@{$self->{'lastPrinting'}}, $self->{'printing'});
    $self->{'printing'} = $value && $self->{'printing'};
}

sub pop {
    my $self = shift;
    $self->{'printing'} = pop(@{$self->{'lastPrinting'}});
    return pop(@{$self->{'values'}});
}

sub enabled {
    my $self = shift;
    return $self->{'printing'};
}

sub disabled {
    my $self = shift;
    return not $self->{'printing'};
}

sub filter {
    my $self = shift;
    my($filter, $value) = @_;
    die "not a valid filter name: '$filter'\n" if $filter =~ m/\W/;
    $self->{'filters'}->{$filter} = $value;
}

sub expand {
    my $self = shift;
    my($line) = @_;
    $line =~ s/__(\w+)__/$self->get($1)/gose;
    return $line;
}

sub print {
    my $self = shift;
    return if $self->{'dependencies'};
    CORE::print(@_);
}

sub visit {
    my $self = shift;
    my($filename) = @_;
    my $directory = $stack->{'variables'}->{'DIRECTORY'};
    $filename = File::Spec->abs2rel(File::Spec->rel2abs($filename, $directory));
    CORE::print("$filename\n");
}

########################################################################

package preprocessor;

sub define {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    my $argument = shift;
    for ($argument) {
        /^(\w+)\s(.*)$/os && do {
            return $stack->define($1, $2);
        };
        /^(\w+)$/os && do {
            return $stack->define($1, 1);
        };
        die "invalid argument: '$_'\n";
    }
}

sub undef {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    $stack->undefine(@_);
}

sub ifdef {
    my $stack = shift;
    die "argument expected\n" unless @_;
    $stack->push($stack->defined(@_));
}

sub ifndef {
    my $stack = shift;
    die "argument expected\n" unless @_;
    $stack->push(not $stack->defined(@_));
}

sub if {
    my $stack = shift;
    die "argument expected\n" unless @_;
    my $argument = shift;
    for ($argument) {
        /^(\w+)==(.*)$/os && do {
            # equality
            return $stack->push($stack->get($1) eq $2);
        };
        /^(\w+)!=(.*)$/os && do {
            # inequality
            return $stack->push($stack->get($1) ne $2);
        };
        /^(\w+)$/os && do {
            # true value
            return $stack->push($stack->get($1));
        };
        /^!(\w+)$/os && do {
            # false value
            return $stack->push(not $stack->get($1));
        };
        die "invalid argument: '$_'\n";
    }
}

sub else {
    my $stack = shift;
    die "argument unexpected\n" if @_;
    $stack->push(not $stack->pop);
}

sub elif {
    my $stack = shift;
    die "argument expected\n" unless @_;
    if ($stack->pop) {
        $stack->push(0);
    } else {
        &if($stack, @_);
    }
}

sub elifdef {
    my $stack = shift;
    die "argument expected\n" unless @_;
    if ($stack->pop) {
        $stack->push(0);
    } else {
        &ifdef($stack, @_);
    }
}

sub elifndef {
    my $stack = shift;
    die "argument expected\n" unless @_;
    if ($stack->pop) {
        $stack->push(0);
    } else {
        &ifndef($stack, @_);
    }
}

sub endif {
    my $stack = shift;
    die "argument unexpected\n" if @_;
    $stack->pop;
}

sub error {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    my $line = $stack->expand(@_);
    die "$line\n";
}

sub expand {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    my $line = $stack->expand(@_);
    $stack->print("$line\n");
}

sub literal {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    my $line = shift;
    $stack->print("$line\n");
}

sub include {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    my $filename = File::Spec->catpath(File::Spec::Unix->splitpath(@_));
    if ($stack->{'dependencies'}) {
        $stack->visit($filename);
    } else {
        main::include($stack, $filename);
    }
}

sub filter {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    foreach (split(/\s/os, shift)) {
        $stack->filter($_, 1);
    }
}

sub unfilter {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    foreach (split(/\s/os, shift)) {
        $stack->filter($_, 0);
    }
}


########################################################################

package filter;

sub spaces {
    my($stack, $text) = @_;
    $text =~ s/ +/ /gos; # middle spaces
    $text =~ s/^ //gos; # start spaces
    $text =~ s/ (\n?)$/$1/gos; # end spaces
    return $text;
}

sub slashslash {
    my($stack, $text) = @_;
    $text =~ s|//.*?(\n?)$|$1|gos;
    return $text;
}

sub substitution {
    my($stack, $text) = @_;
    $text =~ s/@(\w+)@/$stack->get($1, 1)/gose;
    return $text;
}

sub attemptSubstitution {
    my($stack, $text) = @_;
    $text =~ s/@(\w+)@/$stack->get($1, 0)/gose;
    return $text;
}

########################################################################

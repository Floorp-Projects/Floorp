#!/usr/bin/perl -w
# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# Preprocessor
# Version 1.0
#
# Copyright (c) 2002 by Ian Hickson
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
    } elsif (/^-I(.*)$/os) { 
        push(@includes, $1);
    } elsif (/^-E$/os) { 
        foreach (keys %ENV) {
            $stack->define($_, $ENV{$_});
        }
    } else {
        die "$0: invalid argument: $_\n";
    }
}
unshift(@ARGV, '-') unless @ARGV;
unshift(@ARGV, @includes);

# do the work
include($stack, $_) foreach (@ARGV);
exit(0);

########################################################################

package main;

sub include {
    my($stack, $filename) = @_;
    local $stack->{'variables'}->{'FILE'} = $filename;
    local $stack->{'variables'}->{'LINE'} = 0;
    local *FILE;
    open(FILE, $filename);
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
        } else {
            # print it, including any newlines
            print $_ if $stack->enabled;
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
            'FILE' => '', # source filename
            '1' => 1, # for convenience
        },
        'values' => [], # the value of the last condition evaluated at the nth lewel
        'lastPrinting' => [], # whether we were printing at the n-1th level
        'printing' => 1, # whether we are currently printing at the Nth level
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
    my($variable) = @_;
    die "not a valid variable name: '$variable'\n" if $variable =~ m/\W/;
    my $value = $self->{'variables'}->{$variable};
    if (defined($value)) {
        return $value;
    } else {
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
    local $" = ' ';
    die "@_\n";
}

sub expand {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    my $line = shift;
    $line =~ s/__(\w+)__/$stack->get($1)/gose;
    print "$line\n";
}

sub literal {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    my $line = shift;
    print "$line\n";
}

sub include {
    my $stack = shift;
    return if $stack->disabled;
    die "argument expected\n" unless @_;
    main::include($stack, @_);
}

########################################################################

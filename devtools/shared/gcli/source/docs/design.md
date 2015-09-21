
# The Design of GCLI

## Design Goals

GCLI should be:

- primarily for technical users.
- as fast as a traditional CLI. It should be possible to put your head down,
  and look at the keyboard and use GCLI 'blind' at full speed without making
  mistakes.
- principled about the way it encourages people to build commands. There is
  benefit from unifying the underlying concepts.
- automatically helpful.

GCLI should not attempt to:

- convert existing GUI users to a CLI.
- use natural language input. The closest we should get to natural language is
  thinking of commands as ```verb noun --adjective```.
- gain a touch based interface. Whilst it's possible (even probable) that touch
  can provide further benefits to command line users, that can wait while we
  catch up with 1985.
- slavishly follow the syntax of existing commands, predictability is more
  important.
- be a programming language. Shell scripts are mini programming languages but
  we have JavaScript sat just next door. It's better to integrate than compete.


## Design Challenges

What has changed since 1970 that might cause us to make changes to the design
of the command line?


### Connection limitations

Unix pre-dates the Internet and treats almost everything as a file. Since the
Internet it could be more useful to use URIs as ways to identify sources of data.


### Memory limitations

Modern computers have something like 6 orders of magnitude more memory than the
PDP-7 on which Unix was developed. Innovations like stdin/stdout and pipes are
ways to connect systems without long-term storage of the results. The ability
to store results for some time (potentially in more than one format)
significantly reduces the need for these concepts. We should make the results
of past commands addressable for re-use at a later time.

There are a number of possible policies for eviction of items from the history.
We should investigate options other than a simple stack.


### Multi-tasking limitations

Multi-tasking was a problem in 1970; the problem was getting a computer to do
many jobs on 1 core. Today the problem is getting a computer to do one job on
many cores. However we're stuck with this legacy in 2 ways. Firstly that the
default is to force everything to wait until the previous job is finished, but
more importantly that output from parallel jobs frequently collides

    $ find / -ctime 5d -print &
    $ find / -uid 0 -print &
    // good luck working out what came from where

    $ tail -f logfile.txt &
    $ vi main.c
    // have a nice time editing that file

GCLI should allow commands to be asynchronous and will provide UI elements to
inform the user of job completion. It will also keep asynchronous command
output contained within it's own display area.


### Output limitations

The PDP-7 had a teletype. There is something like 4 orders of magnitude more
information that can be displayed on a modern display than a 80x24 character
based console. We can use this flexibility to provide better help to the user
in entering their command.

The additional display richness can also allow interaction with result output.
Command output can include links to follow-up commands, and even ask for
additional input. (e.g. "your search returned zero results do you want to try
again with a different search string")

There is no reason why output must be static. For example, it could be
informative to see the results of an "ls" command alter given changes made by
subsequent commands. (It should be noted that there are times when historical
information is important too)


### Integration limitations

In 1970, command execution meant retrieving a program from storage, and running
it. This required minimal interaction between the command line processor and
the program being run, and was good for resource constrained systems.
This lack of interaction resulted in the processing of command line arguments
being done everywhere, when the task was better suited to command line.
We should provide metadata about the commands being run, to allow the command
line to process, interpret and provide help on the input.

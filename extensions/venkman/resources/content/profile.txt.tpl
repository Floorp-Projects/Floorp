
                            Venkman Profile Report

Created .......... $full-date
User Agent ....... $user-agent
Debugger Version . $venkman-agent
Sorted By ........ $sort-key

=================================================================================
@-section-start
$section-number <$full-url>

@-range-start
  $file-name: $range-min - $range-max milliseconds
@-item-start
    Function Name: $function-name  (Lines $start-line - $end-line)
    Total Calls: $call-count (max recurse $recurse-depth)
    Total Time: $total-time (min/max/avg $min-time/$max-time/$avg-time)

@-item-end
  -------------------------------------------------------------------------------

@-range-end
=================================================================================

@-section-end


Thanks for using Venkman, the Mozilla JavaScript Debugger.
<http://www.mozilla.org/projects/venkman>

<?xml version="1.0"?>
<?xml-stylesheet href="chrome://venkman/content/profile.xml" type="text/xsl"?>
<profile xmlns="http://www.mozilla.org/venkman/0.9/profiler"
         collected="$full-date"
         useragent="$user-agent"
         version="$venkman-agent"
         sortkey="$sort-key">
@-section-start
	<section section="S$section-number"
             prevsection="S$section-number-prev"
             nextsection="S$section-number-next"
             href="$full-url"
             filename="$file-name">
@-range-start
		<range range="S$section-number:$range-number"
               prevsection="S$section-number-prev"
               nextsection="S$section-number-next"
               prevrange="S$section-number:$range-number-prev"
               nextrange="S$section-number:$range-number-next"
               min="$range-min"
               max="$range-max">
@-item-start
			<item item="S$section-number:$range-number:$item-number"
                  itemnumber="$item-number"
                  summary="$item-summary"
                  minpercent="$item-min-pct"
                  belowpercent="$item-below-pct"
                  abovepercent="$item-above-pct"
                  mintime="$min-time"
                  maxtime="$max-time"
                  totaltime="$total-time"
                  callcount="$call-count"
                  function="$function-name"
                  filename="$file-name"
                  fileurl="$full-url"
                  startline="$start-line"
                  endline="$end-line"/>
@-item-end
        </range>
@-range-end
    </section>
@-section-end
</profile>

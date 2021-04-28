BEGIN {
    name_found = 1
    SECTION_DOC = 0
    PUBLIC_DOC = 1
    PRIVATE_DOC = 2
}

function log_msg(severity, msg)
{
    printf "%s (%d): %s: %s %s\n", FILENAME, FNR, severity, doc_name, msg
}

function log_error(msg)
{
    log_msg("ERROR", msg)
}

function log_warning(msg)
{
    log_msg("WARNING", msg)
}

/^\/\*\*$/ {
    in_doc = 1
    doc_line = 0
}

/^(\/\*\*$| \*[ \t]| \*$| \*\*\/$)/ {
    valid_doc = 1
}

in_doc {
    if (!valid_doc)
	log_error("bad line: '" $0 "'")
    valid_doc = 0

    doc_line++
    if (doc_line == 2) {
	# new doc name. Did we find the previous one?
	# (macros are not expected to be found in the same place as
	# their documentation)
	if (!name_found && doc_name !~ /CAIRO_/)
	    log_warning("not found")
	doc_name = $2
	if (doc_name ~ /^SECTION:.*$/) {
	    doc_type = SECTION_DOC
	    name_found = 1
	} else if (tolower(doc_name) ~ /^cairo_[a-z0-9_]*:$/) {
	    doc_type = PUBLIC_DOC
	    name_found = 0
	    real_name = substr(doc_name, 1, length(doc_name) - 1)
	} else if (tolower(doc_name) ~ /^_[a-z0-9_]*:$/) {
	    doc_type = PRIVATE_DOC
	    name_found = 0
	    real_name = substr(doc_name, 1, length(doc_name) - 1)
	} else {
	    log_error("invalid doc id (should be 'cairo_...:')")
	    name_found = 1
	}
    }
}

!in_doc {
    regex = "(^|[ \\t\\*])" real_name "([ ;()]|$)"
    if ($0 ~ regex)
	name_found = 1
}

/^ \* Since: ([0-9]*.[0-9]*|TBD)$/ {
    if (doc_has_since != 0) {
	log_error("Duplicate 'Since' field")
    }
    doc_has_since = doc_line
}

/^ \*\*\// {
    if (doc_type == PUBLIC_DOC) {
	if (!doc_has_since) {
	    # private types can start with cairo_
	    if (doc_name ~ /^cairo_.*_t:$/)
		log_warning("missing 'Since' field (is it a private type?)")
	    else
		log_error("missing 'Since' field")
	} else if (doc_has_since != doc_line - 1)
	    log_warning("misplaced 'Since' field (should be right before the end of the comment)")
    } else {
	if (doc_has_since)
	    log_warning("'Since' field in non-public element")
    }

    in_doc = 0
    doc_has_since = 0
    doc_type = 0
}

/\*\// {
    if (in_doc) {
	in_doc = 0
 	log_error("documentation comment not closed with **/")
    }
}

END {
    if (!name_found)
	log_warning("not found")
}

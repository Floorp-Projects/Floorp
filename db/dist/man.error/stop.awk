BEGIN {
	out = 0
	saved = ""
}
/@START/{
	print saved
	print $0
	out = 1
	next
}
/@STOP/{
	if (out == 1) {
		print $0
		out = 0
	}
	next
}
{
	if (out == 1)
		print $0
	else
		saved = $0
}

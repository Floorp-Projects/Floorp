function owner() 
{
	return email("Norris Boyd", "nboyd", "atg.com");
}

function email(name, prefix, suffix) 
{
	return "<a href='mailto:"+prefix+"@"+suffix+"'>"+name+"</a>";
}

function write_email(name, prefix, suffix) 
{
	document.write(email(name, prefix, suffix));
}


function FillRecipientTypeCombobox()
{
	originalCombo = document.getElementById("msgRecipientType#1");
	if (originalCombo)
	{
		MAX_RECIPIENTS = 2;
	    while ((combo = document.getElementById("msgRecipientType#" + MAX_RECIPIENTS)))
	    {
	    	for (var j = 0; j < originalCombo.length; j ++)
				combo.add(new Option(originalCombo.options[j].text,
									 originalCombo.options[j].value), null);
			MAX_RECIPIENTS++;
		}
		MAX_RECIPIENTS--;
	}
}

function Recipients2CompFields(msgCompFields)
{
	if (msgCompFields)
	{
		var i = 1;
		var addrTo = "";
		var addrCc = "";
		var addrBcc = "";
		var addrReply = "";
		var addrNg = "";
		var addrFollow = "";
		var addrOther = "";
		var to_Sep = "";
		var cc_Sep = "";
		var bcc_Sep = "";
		var reply_Sep = "";
		var ng_Sep = "";
		var follow_Sep = "";

	    while ((inputField = document.getElementById("msgRecipient#" + i)))
	    {
	    	fieldValue = inputField.value;
	    	if (fieldValue != "")
	    	{
	    		switch (document.getElementById("msgRecipientType#" + i).value)
	    		{
	    			case "addr_to"			: addrTo += to_Sep + fieldValue; to_Sep = ",";					break;
	    			case "addr_cc"			: addrCc += cc_Sep + fieldValue; cc_Sep = ",";					break;
	    			case "addr_bcc"			: addrBcc += bcc_Sep + fieldValue; bcc_Sep = ",";				break;
	    			case "addr_reply"		: addrReply += reply_Sep + fieldValue; reply_Sep = ",";			break;
	    			case "addr_newsgroups"	: addrNg += ng_Sep + fieldValue; ng_Sep = ",";					break;
	    			case "addr_followup"	: addrFollow += follow_Sep + fieldValue; follow_Sep = ",";		break;
                   		case "addr_other"	: addrOther += other_header + ": " + fieldValue + "\n"; 		                break;
	    		}
	    	}
	    	i ++;
	    }
    	msgCompFields.SetTo(addrTo);
    	msgCompFields.SetCc(addrCc);
    	msgCompFields.SetBcc(addrBcc);
    	msgCompFields.SetReplyTo(addrReply);
    	msgCompFields.SetNewsgroups(addrNg);
    	msgCompFields.SetFollowupTo(addrFollow);
        msgCompFields.SetOtherRandomHeaders(addrOther);
	}
	else
		dump("Message Compose Error: msgCompFields is null (ExtractRecipients)");
}

function CompFields2Recipients(msgCompFields)
{
	if (msgCompFields)
	{
		var i = 1;
		var fieldValue = msgCompFields.GetTo();
		if (fieldValue != "" && i <= MAX_RECIPIENTS)
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_to";
			i ++;
		}

		fieldValue = msgCompFields.GetCc();
		if (fieldValue != "" && i <= MAX_RECIPIENTS)
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_cc";
			i ++;
		}

		fieldValue = msgCompFields.GetBcc();
		if (fieldValue != "" && i <= MAX_RECIPIENTS)
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_bcc";
			i ++;
		}		

		fieldValue = msgCompFields.GetReplyTo();
		if (fieldValue != "" && i <= MAX_RECIPIENTS)
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_reply";
			i ++;
		}		

		fieldValue = msgCompFields.GetOtherRandomHeaders();
		if (fieldValue != "")
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_other";
			i ++;
		}		

		fieldValue = msgCompFields.GetNewsgroups();
		if (fieldValue != "" && i <= MAX_RECIPIENTS)
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_newsgroups";
			i ++;
		}
		
		fieldValue = msgCompFields.GetFollowupTo();
		if (fieldValue != "" && i <= MAX_RECIPIENTS)
		{
			document.getElementById("msgRecipient#" + i).value = fieldValue;
			document.getElementById("msgRecipientType#" + i).value = "addr_followup";
			i ++;
		}		

		for (; i <= MAX_RECIPIENTS; i++) 
		{ 
			document.getElementById("msgRecipient#" + i).value = ""; 
			document.getElementById("msgRecipientType#" + i).value = "addrTo"; 
		}
	}
}

function AddressingWidgetClick()
{
	dump("AddressingWidgetClick\n");
}

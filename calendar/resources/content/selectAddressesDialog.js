/* INSERT OPEN OEONE LICENSE */


/***** contactsSelectAddresses
* AUTHORS
*   Mark Wood
*
* REQUIRED INCLUDES
*
* NOTES
*   Code for the 'Select Addresses' dialog called from 
*   an email compose dialog
*  
* IMPLEMENTATION NOTES 
*
**********
*/


/** Dialog elements */
const kADDRESS_BOOK_MENU_LIST = "addressBookList";
const kADDRESS_BOOK_MENU_POPUP = "addressBookList-menupopup";
const kCONTACT_CARDS_TREE = "abResultsTree";
const kMAIL_TO_COMMAND_ID_TO = "addToAddress";
const kMAIL_TO_COMMAND_ID_CC = "addCcAddress";
const kMAIL_TO_COMMAND_ID_BCC = "addBccAddress";
const kREMOVE_ADDRESS_COMMAND_ID = "removeAddress";
const kRECIPIENT_LIST_TREE = "addressBucket";
const kRECIPIENT_LIST_TREE_CHILDREN = "bucketBody";
const kRECIPIENT_LIST_CELL_ATTRIBUTE_ADDRESS_PREFIX = "addressPrefix";
const kRECIPIENT_LIST_CELL_ATTRIBUTE_EMAIL = "email";

/** Event dialog elements */
const kINVITE_COMMAND_ID = "addToInviteList";
const kINVITE_REMOVE_COMMAND_ID = "removeFromInviteList";
const kINVITE_LIST_CELL_ATTRIBUTE_CARD_ID = "contactId";
const kINVITE_LIST_ITEM_ATTRIBUTE_CARD_ID = "contactId";

/** Address prefixes */
const kADDRESS_PREFIX_TO = "to";
const kADDRESS_PREFIX_CC = "cc";
const kADDRESS_PREFIX_BCC = "bcc";

/** Global instance of Contact card tree */
var gContactsTree;

/** Global instance of Contacts Datasource */
var gContactsDataSource;

/** Command arrays */
var gMailToCommandsArray;
var gRemoveCommandsArray;
var gInviteCommandsArray;
var gInviteRemoveCommandsArray;

/** Global instance of RDF service */
var gRdf = Components.classes[ "@mozilla.org/rdf/rdf-service;1" ].getService( Components.interfaces.nsIRDFService );
if( Components.classes[ "@mozilla.org/messenger/headerparser;1" ] )
    var gHeaderParser = Components.classes[ "@mozilla.org/messenger/headerparser;1" ].getService( Components.interfaces.nsIMsgHeaderParser );

/** Global refs to original composer window */
var gMsgCompFields = 0;
var gComposeWindow;
var gTopWindow;

/** Array of nsIAbCards for attaching to event */
var gEventCardArray = new Array();


/***** onLoadSelectAddresses
* METHODS
*   onLoadSelectAddresses:  initializes the select addresses dialog
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function onLoadSelectAddresses()
{
    // Set the dialog buttons
    setupOkCancelButtons( selectAddressesOkButton, 0 );

    // Setup the the address book chooser
    setupAddressBookChooser();

    // Setup the commands
    initCommands();

    if( window.arguments && window.arguments[ 0 ] )
    {
        // Get the Contacts Data Source
        gContactsDataSource = window.arguments[ 0 ].contactsDataSource;

        // Get the uri of the first address book in the chooser
        var bookUri = document.getElementById( kADDRESS_BOOK_MENU_LIST ).getAttribute( "value" );
        
        // Generate the cardset for the specified book
        //updateCardSet( gContactsDataSource, bookUri );
        updateSelectAddressesCardSet( gContactsDataSource, bookUri )

        // Get the compose window and associated elements
        var toAddress = "", ccAddress = "", bccAddress = "";

        gComposeWindow = window.arguments[0].composeWindow;
        gMsgCompFields = window.arguments[ 0 ].msgCompFields;
        gTopWindow = window.arguments[ 0 ].topWindow;
        toAddress = window.arguments[ 0 ].toAddress;
        ccAddress = window.arguments[ 0 ].ccAddress;
        bccAddress = window.arguments[ 0 ].bccAddress;

        // Add any existing addresses to the recipient list
        addAddressFromComposeWindow( toAddress, DTD_toPrefix, kADDRESS_PREFIX_TO );
        addAddressFromComposeWindow( ccAddress, DTD_ccPrefix, kADDRESS_PREFIX_CC );
        addAddressFromComposeWindow( bccAddress, DTD_bccPrefix, kADDRESS_PREFIX_BCC );
    }
}   // End function onLoadSelectAddresses



/***** addAddressFromComposeWindow
* METHODS
*   addAddressFromComposeWindow:    adds existing addresses from the
*                                   compose window to the list of
*                                   recipients
*
* PARAMETERS
*   addresses:  comma-separated list of email addresses
*   displayPrefix:  prefix to display with addresses
*   addressPrefix:  mailto prefix to use with addresses
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function addAddressFromComposeWindow( addresses, displayPrefix, addressPrefix )
{
    if( addresses.length > 0 )
    {
        var addressArray = addresses.split(",");
    
        for( var index = 0; index < addressArray.length; index++ )
        {
            // remove leading spaces
            while ( addressArray[index][0] == " " )
            {
                addressArray[ index ] = addressArray[ index ].substring( 1, addressArray[ index ].length );
            }
            
            addAddressIntoBucket( addressPrefix, ( displayPrefix + addressArray[ index ] ), addressArray[ index ] );
        }
    }
}   // End function addAddressFromComposeWindow



/***** setupAddressBookChooser
* METHODS
*   setupAddressBookChooser:    initializes the address book chooser
*                               by selecting the first address book
*                               in the list
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function setupAddressBookChooser()
{
    // Select the first item in the menulist
	var addressBookChooser = document.getElementById( kADDRESS_BOOK_MENU_LIST );
    
	if( addressBookChooser )
	{
        // Default to the first valid addressbook when none is
        // selected. (the 0th is an empty/invalid entry)
		var menupopup = document.getElementById( kADDRESS_BOOK_MENU_POPUP );
        addressBookChooser.label = menupopup.childNodes[ 1 ].getAttribute( "label" );
        addressBookChooser.value = menupopup.childNodes[ 1 ].getAttribute( "value" );
	}
}   // End function setupAddressBookChooser



/***** onChooseAddressBook
* METHODS
*   onChooseAddressBook:    called when an address book is selected 
*                           from the address book chooser
*
* PARAMETERS
*   addressBookMenuList:    menulist item containing address book ids
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function onChooseAddressBook( addressBookListMenuList )
{
    // Get the uri of the selected address book
    var bookUri = addressBookListMenuList.getAttribute( "value" );

    // Generate the cardset for the specified book
    //updateCardSet( gContactsDataSource, bookUri );
    updateSelectAddressesCardSet( gContactsDataSource, bookUri )

    // Update the commands
    updateCommands( gMailToCommandsArray, true );
}   // End function onChooseAddressBook



/***** updateCardSet
* METHODS
*   updateCardSet:  updates the current cardset for the specified
*                   address book uri
*
* PARAMETERS
*   dataSource: instance of Contacts datasource
*   bookUri:    uri of specified address book
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function updateCardSet( dataSource, bookUri )
{
    // Get the card set for the selected address book
    var targetBook = dataSource.makeAddressBook( bookUri );
    var cardSet = dataSource.makeCardSet( "GeneratedName", "ascending" );
    cardSet.initWithAddressBookUri( targetBook.getBookUri() );
    
    // Setup the tree displaying the contacts for the selected address book
    //gContactsTree = new ContactsTree( cardSet );
    return cardSet;
}   // End function updateCardSet

function updateSelectAddressesCardSet( dataSource, bookUri )
{
    var cardSet = updateCardSet( dataSource, bookUri );
    gContactsTree = new ContactsTree( cardSet );
}


function updateEventCardSet( dataSource, bookUri )
{
    var cardSet = updateCardSet( dataSource, bookUri );
    gContactsTree = new EventContactsTree( cardSet );
}



/***** ContactsTree
* METHODS
*   ContactsTree:   definition for tree used to display contact cards
*
* PARAMETERS
*   cardSet:    CardSet object containing cards for selected address book   
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function ContactsTree( cardSet )
{
    this.cardSet = cardSet;
    
    this.resultsTree = document.getElementById( kCONTACT_CARDS_TREE );
    this.resultsTree.treeBoxObject.view = this;
    this.resultsTree.contactsTree = this;

    this.atomRowEnabled = makeAtom( "enabled" );
    this.atomRowDisabled = makeAtom( "disabled" );
}   // End ContactsTree definition


/***** ContactsTree
* METHODS
*   Method definition for ContactsTree object
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
ContactsTree.prototype = 
{
    // TREE Functions
    mTree: null,
    mSelection: null,

    get rowCount() 
    { 
        return this.cardSet.cardList.length; 
    },

    get selection() { return this.mSelection; },

    set selection(aSelection) { this.mSelection = aSelection; },
    setTree: function(aTree) { this.mTree = aTree; },
    getCellText: function(aRow, aCol) 
    { 
        var card = this.cardSet.cardList[ aRow ];

        if( card )
        {
            if( aCol.id == "PrimaryEmail" && !card.isMailList() )
            {
                if( card.getCardValue( aCol.id ).length == 0 )
                {
                    return DTD_noEmailMessage;
                }

                else
                {
                    return card.getCardValue( aCol.id );
                }
            }

            else
            {
                return card.getCardValue( aCol.id );
            }
        }

        else
        {
            return "";
        }
    },
    
    getRowProperties: function(aIndex, aProperties)
    {
        var card = this.cardSet.cardList[ aIndex ];
         
        if( !card.isMailList() )
        {
            if( card.getCardValue( "PrimaryEmail" ) == "" )
            {
                aProperties.AppendElement( this.atomRowDisabled  );
            }
            else
            {
                aProperties.AppendElement( this.atomRowEnabled  );
            }
        }
    },

    getCellProperties: function(aRow, aCol, aProperties) 
    {
        var card = this.cardSet.cardList[ aRow ];
        
        if( !card.isMailList() )
        {
            if( card.getCardValue( "PrimaryEmail" ) == "" )
            {
                aProperties.AppendElement( this.atomRowDisabled  );
            }
            else
            {
                aProperties.AppendElement( this.atomRowEnabled  );
            }
        }
    },
    
    getColumnProperties: function(aCol, aProperties) {},
    getParentIndex: function(aRowIndex) { return -1; },
    //hasNextSibling: function(aRowIndex, aAfterIndex) { },
    getLevel: function(aIndex) { return 0; },
    getImageSrc: function(aRow, aCol) {},
    //getProgressMode: function(aRow, aCol) {},
    //getCellValue: function(aRow, aCol) {},
    isContainer: function(aIndex) { return false; },
    //isContainerOpen: function(aIndex) {},
    //isContainerEmpty: function(aIndex) {},
    isSeparator: function(aIndex) { return false; },
    isSorted: function() {},
    //toggleOpenState: function(aIndex) {},
    
    selectionChanged: function() 
    {
    },
    
    
    cycleHeader: function(aCol) {},
    //cycleCell: function(aRow, aCol) {},
    //isEditable: function(aRow, aCol) {},
    setCellValue: function(aRow, aCol, aValue) {},
    setCellText: function(aRow, aCol, aValue) {},
    //performAction: function(aAction) {},
    //performActionOnRow: function(aAction, aRow) {},
    //performActionOnCell: function(aAction, aRow, aCol) {},
    
    
    // extra utility stuff
    
    setSelection : function( selList )
    {
        this.selection.clearSelection();
        
        for( var i = 0; i < selList.length; ++i )
        {
            this.selection.rangedSelect( selList[i], selList[i], true );
        }
    },
    
    getSelectedCardIndexes : function(  )
    {
        var selectedCardIndexes = new Array( this.selection.count );
        
        var count = this.selection.getRangeCount();
        
        var current = 0;
        
        var start = new Object();
        var end = new Object();
        
        for( var i = 0; i < count; i++) 
        {
            this.selection.getRangeAt(i,start,end);
            
            for( var j = start.value; j <= end.value; j++ ) 
            {
                selectedCardIndexes[current] = j;
                current++;
            }
        }
        
        return selectedCardIndexes;
    },
        
    onMouseDown : function( event )
    {
        // we only care about button 0 (left click) events
        if (event.button != 0) return;
        
        // all we need to worry about here is double clicks
        // and column header clicks.
        //
        // we get in here for clicks on the "treecol" (headers)
        // and the "scrollbarbutton" (scrollbar buttons)
        // we don't want those events to cause a "double click"
        
        var target = event.originalTarget;
        
        if (target.localName == "treecol") 
        {
            var columnSortField = target.getAttribute( "sort-field" );
            sortAndUpdateIndicators( columnSortField );
        }
        else if (target.localName == "treechildren") 
        {
            var row = this.recipientTreeElement.treeBoxObject.getRowAt( event.clientX, event.clientY );
            
            if( row != -1 && row < this.recipientTreeElement.view.rowCount )
            {
                var card = this.cardSet.cardList[ row ];
                
                if( card.getPrimaryEmail() != "" )
                {
                    card._checked = !card._checked; 
                    this.recipientTreeElement.treeBoxObject.invalidateRow( row ); 
                }
            }
        }
    },

    onClick: function( event )
    {
        // Get the target
        var target = event.originalTarget;

        if( target.localName == "treechildren" ) 
        {
            // Get the email address of the card just clicked
            var row = this.resultsTree.treeBoxObject.getRowAt( event.clientX, event.clientY );

            if( row != -1 )
            {
                var card = this.cardSet.cardList[ row ];
 
                // If the card has no email address, remove it from the selection
                if( card && ( !card.isMailList() ) && ( card.getCardValue( "PrimaryEmail" ) == "" ) )
                {
                    this.selection.clearRange( row, row );
                    return;
                }
    
                if( this.getSelectedCardIndexes().length > 0 )
                {
                    updateCommands( gMailToCommandsArray, false );
                }
        
                else
                {
                    updateCommands( gMailToCommandsArray, true );
                }
            }
        }
    },

    // If an address is double clicked, add it to the list as a 'To' address
    onDblClick: function( event )
    {
        addSelectedAddressesIntoBucket( DTD_toPrefix, kADDRESS_PREFIX_TO );
    }
};


function makeAtom( aVal )
{
    var svc = Components.classes[ "@mozilla.org/atom-service;1" ].getService( Components.interfaces.nsIAtomService );
    return svc.getAtom(aVal);
}


/***** initCommands
* METHODS
*   initCommands:   calls all required command initializers
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function initCommands()
{
    initMailToCommands();
    initRemoveCommands();
}   // End function initCommands


/***** initMailToCommands
* METHODS
*   initMailToCommands: initializes the commands associated with
*                       selecting existing contacts  
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function initMailToCommands()
{
    gMailToCommandsArray = new Array( kMAIL_TO_COMMAND_ID_TO, kMAIL_TO_COMMAND_ID_CC, kMAIL_TO_COMMAND_ID_BCC );
}   // End function initMailToCommands


/***** initRemoveCommands
* METHODS
*   initRemoveCommands: initializes the commands associated with
*                       selecting recipients  
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function initRemoveCommands()
{
    gRemoveCommandsArray = new Array( kREMOVE_ADDRESS_COMMAND_ID );
}   // End function initRemoveCommands


/***** updateCommands
* METHODS
*   updateCommands: updates specified commands associated with 
*                   specified status  
*
* PARAMETERS
*   commandsArray:  Array containing command ids
*   disableStatus:  true to disable commands, false to enable
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function updateCommands( commandsArray, disableStatus )
{
    if( commandsArray )
    {
        var element;

        for( var index = 0; index < commandsArray.length; index++ )
        {
            element = document.getElementById( commandsArray[ index ] );
    
            if( element )
            {
                element.setAttribute( "disabled", disableStatus );
            }
        }
    }
}   // End function updateCommands


/***** addSelectedAddressesIntoBucket
* METHODS
*   addSelectedAddressesIntoBucket: adds the selected addresses to the 
*                                   recipient list using the specified
*                                   prefix  
*
* PARAMETERS
*   displayPrefix:  display prefix to use with selected addresses  
*   addressPrefix:  address prefix to use with selected addresses
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function addSelectedAddressesIntoBucket( displayPrefix, addressPrefix )
{
    addSelectedCardsToBucket( displayPrefix, addressPrefix );

    // Clear selection and update mailto commands
    gContactsTree.resultsTree.view.selection.clearSelection();
    updateCommands( gMailToCommandsArray, true );
}   // End function addSelectedAddressesIntoBucket


function addSelectedCardsToBucket( displayPrefix, addressPrefix )
{
    var cards = getSelectedCards();
    var count = cards.length;
    
    for( var i = 0; i < count; i++ )
    {
        addCardIntoBucket( displayPrefix, addressPrefix, cards[ i ] );
    }
}


function addSelectedInviteCardsToBucket( displayPrefix, addressPrefix )
{
    var cards = getSelectedCards();
    var count = cards.length;
    var cardId;
    
    for( var i = 0; i < count; i++ )
    {
        var address = displayPrefix + getAddressFromCard( cards[ i ] );
        
        if( !cards[ i ].isMailList )
        {
            // Add the card to the display
            cardId = generateCardId( cards[ i ] );
            addInviteAddressIntoBucket( address, cardId );
        
            // Add to the event card array as well
            gEventCardArray[ cardId ] = cards[ i ];
        }
    }
}


/***** generateCardId
* METHODS
*   generateCardId: returns a unique card id using the key of the
*                   specified nsIAbCard and its associated 
*                   address book id 
*
* PARAMETERS
*   card:   instance of nsIAbCard
*   [ bookId ]: uri of associated address book; uses current book if none specified 
*
* RETURN VALUE
*   String: unique card identifier
*
* NOTES
**********
*/
function generateCardId( card, bookId )
{
    var cardId;

    // If no bookId specified, get the current address book uri
    if( !bookId )
    {
        bookId = document.getElementById( kADDRESS_BOOK_MENU_LIST ).getAttribute( "value" );
    }

    cardId = bookId + "/Card" + card.key;

    return cardId;        
}   // End function generateCardId



/***** addCardIntoBucket
* METHODS
*   addCardIntoBucket:  adds the specified card to the recipient
*                       list with the specified prefix  
*
* PARAMETERS
*   displayPrefix:  address prefix to display
*   addressPrefix:  address prefix to use 
*   card:   nsIAbCard to add to recipient list  
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function addCardIntoBucket( displayPrefix, addressPrefix, card )
{
    var address = displayPrefix + getAddressFromCard( card );

    if( card.isMailList )
    {
        addAddressIntoBucket( addressPrefix, address, getAddressFromCard( card ) );
    }

    else
    {
        addAddressIntoBucket( addressPrefix, address, card.primaryEmail );
    }
}   // End function addCardIntoBucket


function getAddressFromCard( card )
{
    var email;
    
    if( card.isMailList ) 
    {
        var directory = getDirectoryFromURI(card.mailListURI);

        if( directory.description )
        {
            email = directory.description;
        }
    
        else
        {
            email = card.displayName;
        }
    }

    else
    {
        email = card.primaryEmail;
    }
    
    email = gHeaderParser.makeFullAddressWString( card.displayName, email );
    return email;
}
   
function getDirectoryFromURI( uri )
{
    var directory = gRdf.GetResource( uri ).QueryInterface( Components.interfaces.nsIAbDirectory );
    return directory;
}

/***** getSelectedCards
* METHODS
*   getSelectedCards:   returns an array of selected address cards  
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   Array containing address cards
*
* NOTES
**********
*/
function getSelectedCards()
{
    var cards = new Array();
    var selectedIndexes = gContactsTree.getSelectedCardIndexes();

    if( selectedIndexes.length > 0 )
    {
        for( var index = 0; index < selectedIndexes.length; index++ )
        {
            cards.push( gContactsTree.cardSet.cardList[ selectedIndexes[ index ] ].abCard );
        }
    }

    return cards;
}   // End function getSelectedCards


/***** addAddressIntoBucket
* METHODS
*   addAddressIntoBucket:   add the address to the recipient
*                           display  
*
* PARAMETERS
*   addressPrefix:  address prefix to use 
*   displayAddress: address that gets displayed in recipient list
*   email:  email address to use
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function addAddressIntoBucket( addressPrefix, displayAddress, email )
{
    var body = document.getElementById( kRECIPIENT_LIST_TREE_CHILDREN );
    
    var item = document.createElement( "treeitem" );
    var row = document.createElement( "treerow" );
    var cell = document.createElement( "treecell" );

    cell.setAttribute( "label", displayAddress );
    cell.setAttribute( kRECIPIENT_LIST_CELL_ATTRIBUTE_EMAIL, email );
    cell.setAttribute( kRECIPIENT_LIST_CELL_ATTRIBUTE_ADDRESS_PREFIX, addressPrefix );
    
    row.appendChild( cell );
    item.appendChild( row );
    body.appendChild( item );
}   // End function addAddressIntoBucket


/***** addInviteAddressIntoBucket
* METHODS
*   addInviteAddressIntoBucket: add the address to the invite list
*
* PARAMETERS
*   displayAddress: address that gets displayed in invite list
*   cardId: unique identifier of associated card in gEventCardArray
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function addInviteAddressIntoBucket( displayAddress, cardId )
{
    var body = document.getElementById( kRECIPIENT_LIST_TREE_CHILDREN );
    
    var item = document.createElement( "treeitem" );
    var row = document.createElement( "treerow" );
    var cell = document.createElement( "treecell" );

    cell.setAttribute( "label", displayAddress );
    item.setAttribute( kINVITE_LIST_ITEM_ATTRIBUTE_CARD_ID, cardId );
    
    row.appendChild( cell );
    item.appendChild( row );
    body.appendChild( item );
}   // End function addInviteAddressIntoBucket


/***** selectRecipient
* METHODS
*   selectRecipient:    called when a recipient is selected or
*                       unselected; used to update appropriate 
*                       commands
*
* PARAMETERS
*   tree:   XUL tree containing recipients
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function selectRecipient( tree )
{
    var selectedItems = getSelectedRecipients( tree );
   
    if( selectedItems.length > 0 )
    {
        updateCommands( gRemoveCommandsArray, false );
    }

    else
    {
        updateCommands( gRemoveCommandsArray, true );
    }
}   // End function selectRecipient


function getSelectedRecipients( tree )
{
    var selectedItems = new Array();
                                      
    if( tree && tree.view && tree.view.selection )
    {
        var rangeCount = tree.view.selection.getRangeCount();
    
        for( var i = 0; i < rangeCount; i++ )
        {
            var startIndex = new Object();
            var endIndex = new Object();
    
            tree.view.selection.getRangeAt( i, startIndex, endIndex );
    
            for( var j = startIndex.value; j <= endIndex.value; j++ )
            {
                selectedItems.push( tree.treeBoxObject.view.getItemAtIndex( j ) );
            }
        }
    }

    return selectedItems;
}


/***** removeSelectedFromBucket
* METHODS
*   removeSelectedFromBucket:   removes selected items from the
*                               specified tree    
*
* PARAMETERS
*   bucketTree: XUL tree containing selected items
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function removeSelectedFromBucket( bucketTree )
{
    if( bucketTree )
    {
        var body = document.getElementById( kRECIPIENT_LIST_TREE_CHILDREN );
        var selection = bucketTree.view.selection;
        var rangeCount = selection.getRangeCount();
        
        for( i = rangeCount-1; i >= 0; --i )
        {
            var start = {}, end = {};
            selection.getRangeAt( i, start, end );

            for( j = end.value; j >= start.value; --j )
            {
                var item = bucketTree.contentView.getItemAtIndex( j );
                body.removeChild( item );
            }
        }
    }
}   // End function removeSelectedFromBucket



/***** removeSelectedFromRecipientBucket
* METHODS
*   removeSelectedFromRecipientBucket:  removes selected addresses
*                                       from recipient list    
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function removeSelectedFromRecipientBucket()
{
    // Remove selected items from recipient list
    var bucketTree = document.getElementById( kRECIPIENT_LIST_TREE );
    removeSelectedFromBucket( bucketTree );

    // Disable remove command
    updateCommands( gRemoveCommandsArray, true );
}   // End function removeSelectedFromRecipientBucket



/***** selectAddressesOkButton
* METHODS
*   selectAddressesOkButton:    adds the selected recipients to the
*                               compose window and closes the select
*                               addresses dialog
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function selectAddressesOkButton()
{
    var body = document.getElementById( kRECIPIENT_LIST_TREE_CHILDREN );
    var item, row, cell, text, mailPrefix, colon, email;
    var toAddress = "", ccAddress = "", bccAddress = "", emptyEmail = "";
    
    for( var index = 0; index < body.childNodes.length; index++ )
    {
        item = body.childNodes[index];

        if( item.childNodes && item.childNodes.length )
        {
            row = item.childNodes[ 0 ];

            if( row.childNodes &&  row.childNodes.length )
            {
                cell = row.childNodes[0];
                mailPrefix = cell.getAttribute( kRECIPIENT_LIST_CELL_ATTRIBUTE_ADDRESS_PREFIX );
                text = cell.getAttribute( "label" );
                email = cell.getAttribute( kRECIPIENT_LIST_CELL_ATTRIBUTE_EMAIL );

                if( mailPrefix )
                {
                    switch( mailPrefix )
                    {
                        case kADDRESS_PREFIX_TO:
                        toAddress += email + ",";
                        break;

                        case kADDRESS_PREFIX_CC:
                        ccAddress += email + ",";
                        break;

                        case kADDRESS_PREFIX_BCC:
                        bccAddress += email + ",";
                        break;
                    }
                }
                
                if(!email)
                {
                    if (emptyEmail)
                    {
                        emptyEmail +=", ";
                    }
                    
                    emptyEmail += text.substring(prefixTo.length, text.length-2);
                }
            }
        }
    }

    if(emptyEmail)
    {
        var alertText = gAddressBookBundle.getString("emptyEmailCard");
        alert(alertText + emptyEmail);
        return false;
    }

    // Trim any trailing commas from the address lists
    if( toAddress[ toAddress.length - 1 ] == "," )
    {
        toAddress = toAddress.substring( 0, ( toAddress.length - 1 ) );
    }

    if( ccAddress[ ccAddress.length - 1 ] == "," )
    {
        ccAddress = ccAddress.substring( 0, ( ccAddress.length - 1 ) );
    }

    if( bccAddress[ bccAddress.length - 1 ] == "," )
    {
        bccAddress = bccAddress.substring( 0, ( bccAddress.length - 1 ) );
    }

    // reset the UI in compose window
    gMsgCompFields.to = toAddress;
    gMsgCompFields.cc = ccAddress;
    gMsgCompFields.bcc = bccAddress;
    gTopWindow.CompFields2Recipients( gMsgCompFields );
    
    // Return true to close OE dialog
    return true;
}   // End function selectAddressesOkButton



/***** onLoadEventDialog
* METHODS
*   onLoadEventDialog:  initializes the 'Contacts' portion of the
*                       Calendar Event dialog
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function onLoadEventDialog()
{
    // Get the Contacts Data Source
    gContactsDataSource = pendialog.getService( "org.penzilla.contacts" ).contactsDataSource;

    // Store any existing attached cards in the event card array
    initializeContactsArray();

    // Display any existing attached contacts
    setupAttachedContacts( gEventCardArray );

    // Setup the the address book chooser
    setupAddressBookChooser();

    // Setup the commands
    initEventAddressesCommands();

    // Get the uri of the first address book in the chooser
    var bookUri = document.getElementById( kADDRESS_BOOK_MENU_LIST ).getAttribute( "value" );

    // Generate the cardset for the specified book
    updateEventCardSet( gContactsDataSource, bookUri );
}   // End function onLoadEventDialog


function getAddressBookDatabase( bookId )
{
    var book = Components.classes[ "@mozilla.org/addressbook;1" ].createInstance( Components.interfaces.nsIAddressBook );
    var bookDatabase = book.getAbDatabaseFromURI( bookId );
    return bookDatabase;
}


/***** initializeContactsArray
* METHODS
*   initializeContactsArray:    initalizes the contacts with any
*                               existing contacts attached to the
*                               event  
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function initializeContactsArray()
{
    var contactsArray = gEvent.contactsArray;
    
    if( contactsArray )
    {
        var bookId;
        var cardId;
        var card;
        var mdbCard;

        for( var i = 0; i < contactsArray.Count(); i++ )
        {
            card = contactsArray.QueryElementAt( i, Components.interfaces.nsIAbCard );
            mdbCard = contactsArray.QueryElementAt( i, Components.interfaces.nsIAbMDBCard );
            bookId = getBookId( card );
            cardId = generateCardId( mdbCard, bookId );
            gEventCardArray[ cardId ] = card;
        }
    }
}   // End function initializeContactsArray



/***** getBookId
* METHODS
*   getBookId:  returns the uri of the containing address book
*               of the specified card  
*
* PARAMETERS
*   card:   instance of nsIAbCard
*
* RETURN VALUE
*   String: uri of containing address book
*
* NOTES
*   nsIAbCard instances no longer contain a unique key across
*   address books, only a key within their book.  To figure out
*   where it belongs, this goes through all the existing books
*   and determines where the card belongs.  If there is a simpler
*   way to do this, please implement it.  This is done when displaying
*   existing attached contacts in an event to get a unique id and avoid
*   adding duplicates and to remove the correct contact from the array
*   Mark Wood, July 23, 2002
**********
*/
function getBookId( card )
{
    // Go through all the address books until a match is found
    var addressBookList = gContactsDataSource.getAllAddressBookUris();
    var bookDatabase;
    var bookId;

    for( var i = 0; i < addressBookList.length; i++ )
    {
        bookDatabase = getAddressBookDatabase( addressBookList[ i ] );

        if( bookDatabase && bookDatabase.containsCard( card ) )
        {
            bookId = addressBookList[ i ];
            break;
        }
    }    

    return bookId;
}   // End function getBookId



/***** EventContactsTree
* METHODS
*   EventContactsTree:   definition for tree used to display contact cards
*
* PARAMETERS
*   cardSet:    CardSet object containing cards for selected address book   
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function EventContactsTree( cardSet )
{
    this.cardSet = cardSet;
    
    this.resultsTree = document.getElementById( kCONTACT_CARDS_TREE );
    this.resultsTree.treeBoxObject.view = this;
    this.resultsTree.contactsTree = this;

    this.atomRowEnabled = makeAtom( "enabled" );
    this.atomRowDisabled = makeAtom( "disabled" );
}   // End EventContactsTree definition


/***** EventContactsTree
* METHODS
*   Method definition for EventContactsTree object
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
EventContactsTree.prototype = 
{
    // TREE Functions
    mTree: null,
    mSelection: null,

    get rowCount() 
    { 
        return this.cardSet.cardList.length; 
    },

    get selection() { return this.mSelection; },

    set selection(aSelection) { this.mSelection = aSelection; },
    setTree: function(aTree) { this.mTree = aTree; },
    getCellText: function(aRow, aCol) 
    { 
        var card = this.cardSet.cardList[ aRow ];

        if( card )
        {
            if( aCol.id == "PrimaryEmail" && !card.isMailList() )
            {
                if( card.getCardValue( aCol.id ).length == 0 )
                {
                    return DTD_noEmailMessage;
                }

                else
                {
                    return card.getCardValue( aCol.id );
                }
            }

            else
            {
                return card.getCardValue( aCol.id );
            }
        }

        else
        {
            return "";
        }
    },
    
    getRowProperties: function(aIndex, aProperties)
    {
        var card = this.cardSet.cardList[ aIndex ];
         
        if( !card.isMailList() )
        {
            if( card.getCardValue( "PrimaryEmail" ) == "" )
            {
                aProperties.AppendElement( this.atomRowDisabled  );
            }
            else
            {
                aProperties.AppendElement( this.atomRowEnabled  );
            }
        }
    },

    getCellProperties: function(aRow, aCol, aProperties) 
    {
        var card = this.cardSet.cardList[ aRow ];
        
        if( !card.isMailList() )
        {
            if( card.getCardValue( "PrimaryEmail" ) == "" )
            {
                aProperties.AppendElement( this.atomRowDisabled  );
            }
            else
            {
                aProperties.AppendElement( this.atomRowEnabled  );
            }
        }
    },
    
    getColumnProperties: function(aCol, aProperties) {},
    getParentIndex: function(aRowIndex) { return -1; },
    //hasNextSibling: function(aRowIndex, aAfterIndex) { },
    getLevel: function(aIndex) { return 0; },
    getImageSrc: function(aRow, aCol) {},
    //getProgressMode: function(aRow, aCol) {},
    //getCellValue: function(aRow, aCol) {},
    isContainer: function(aIndex) { return false; },
    //isContainerOpen: function(aIndex) {},
    //isContainerEmpty: function(aIndex) {},
    isSeparator: function(aIndex) { return false; },
    isSorted: function() {},
    //toggleOpenState: function(aIndex) {},
    
    selectionChanged: function() 
    {
    },
    
    
    cycleHeader: function(aCol) {},
    //cycleCell: function(aRow, aCol) {},
    //isEditable: function(aRow, aCol) {},
    setCellValue: function(aRow, aCol, aValue) {},
    setCellText: function(aRow, aCol, aValue) {},
    //performAction: function(aAction) {},
    //performActionOnRow: function(aAction, aRow) {},
    //performActionOnCell: function(aAction, aRow, aCol) {},
    
    
    // extra utility stuff
    
    setSelection : function( selList )
    {
        this.selection.clearSelection();
        
        for( var i = 0; i < selList.length; ++i )
        {
            this.selection.rangedSelect( selList[i], selList[i], true );
        }
    },
    
    getSelectedCardIndexes : function(  )
    {
        var selectedCardIndexes = new Array( this.selection.count );
        
        var count = this.selection.getRangeCount();
        
        var current = 0;
        
        var start = new Object();
        var end = new Object();
        
        for( var i = 0; i < count; i++) 
        {
            this.selection.getRangeAt(i,start,end);
            
            for( var j = start.value; j <= end.value; j++ ) 
            {
                selectedCardIndexes[current] = j;
                current++;
            }
        }
        
        return selectedCardIndexes;
    },
        
    onMouseDown : function( event )
    {
        // we only care about button 0 (left click) events
        if (event.button != 0) return;
        
        // all we need to worry about here is double clicks
        // and column header clicks.
        //
        // we get in here for clicks on the "treecol" (headers)
        // and the "scrollbarbutton" (scrollbar buttons)
        // we don't want those events to cause a "double click"
        
        var target = event.originalTarget;
        
        if (target.localName == "treecol") 
        {
            var columnSortField = target.getAttribute( "sort-field" );
            sortAndUpdateIndicators( columnSortField );
        }
        else if (target.localName == "treechildren") 
        {
            var row = this.recipientTreeElement.treeBoxObject.getRowAt( event.clientX, event.clientY );
            
            if( row != -1 && row < this.recipientTreeElement.view.rowCount )
            {
                var card = this.cardSet.cardList[ row ];
                
                if( card.getPrimaryEmail() != "" )
                {
                    card._checked = !card._checked; 
                    this.recipientTreeElement.treeBoxObject.invalidateRow( row ); 
                }
            }
        }
    },

    onClick: function( event )
    {
        // Get the target
        var target = event.originalTarget;

        if( target.localName == "treechildren" ) 
        {
            // Get the email address of the card just clicked
            var row = this.resultsTree.treeBoxObject.getRowAt( event.clientX, event.clientY );

            if( row != -1 )
            {
                var card = this.cardSet.cardList[ row ];
 
                // If the card has no email address, remove it from the selection
                if( card && ( !card.isMailList() ) && ( card.getCardValue( "PrimaryEmail" ) == "" ) )
                {
                    this.selection.clearRange( row, row );
                    return;
                }
    
                if( this.getSelectedCardIndexes().length > 0 )
                {
                    updateCommands( gInviteCommandsArray, false );
                }      

                else
                {
                    updateCommands( gInviteCommandsArray, true );
                }
            }
        }
    },

    // If an address is double clicked, add it to the list as a 'To' address
    onDblClick: function( event )
    {
        addSelectedAddressesIntoInviteBucket( '', '' );
    }
};


/***** onChooseAddressBook
* METHODS
*   onChooseAddressBook:    called when an address book is selected 
*                           from the address book chooser
*
* PARAMETERS
*   addressBookMenuList:    menulist item containing address book ids
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function onChooseAddressBookEventDialog( addressBookListMenuList )
{
    // Get the uri of the selected address book
    var bookUri = addressBookListMenuList.getAttribute( "value" );

    // Generate the cardset for the specified book
    updateEventCardSet( gContactsDataSource, bookUri )

    // Update the commands
    updateCommands( gInviteCommandsArray, true );
}   // End function onChooseAddressBook


/***** initEventAddressesCommands
* METHODS
*   initEventAddressesCommands: calls all required command initializers
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function initEventAddressesCommands()
{
    initInviteCommands();
    initRemoveInviteCommands();
}   // End function initEventAddressesCommands



/***** initInviteCommands
* METHODS
*   initInviteCommands: initializes the commands associated with
*                       selecting contacts to invite to event
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function initInviteCommands()
{
    gInviteCommandsArray = new Array( kINVITE_COMMAND_ID );
}   // End function initInviteCommands


/***** initRemoveInviteCommands
* METHODS
*   initRemoveInviteCommands:   initializes the commands associated 
*                               with removing contacts from the invite list
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function initRemoveInviteCommands()
{
    gInviteRemoveCommandsArray = new Array( kINVITE_REMOVE_COMMAND_ID );
}   // End function initRemoveInviteCommands



/***** selectEventRecipient
* METHODS
*   selectEventRecipient:   called when a recipient is selected or
*                           unselected; used to update appropriate 
*                           commands
*
* PARAMETERS
*   tree:   XUL tree containing recipients
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function selectEventRecipient( tree )
{
    var selectedItems = getSelectedRecipients( tree );
   
    if( selectedItems.length > 0 )
    {
        updateCommands( gInviteRemoveCommandsArray, false );
    }

    else
    {
        updateCommands( gInviteRemoveCommandsArray, true );
    }
}   // End function selectEventRecipient


/***** addSelectedAddressesIntoInviteBucket
* METHODS
*   addSelectedAddressesIntoInviteBucket:   adds the selected addresses
*                                           to the recipient list   
*
* PARAMETERS
*   displayPrefix:  display prefix to use with selected addresses  
*   addressPrefix:  address prefix to use with selected addresses
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function addSelectedAddressesIntoInviteBucket( displayPrefix, addressPrefix )
{
    // Add the selected cards
    addSelectedInviteCardsToBucket( displayPrefix, addressPrefix );

    // Clear selection and update mailto commands
    gContactsTree.resultsTree.view.selection.clearSelection();
    updateCommands( gInviteCommandsArray, true );
}   // End function addSelectedAddressesIntoInviteBucket


/***** removeSelectedFromInviteBucket
* METHODS
*   removeSelectedFromInviteBucket: removes selected addresses from the
*                                   invite list    
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function removeSelectedFromInviteBucket()
{
    var bucketTree = document.getElementById( kRECIPIENT_LIST_TREE );

    // Remove the items from the event card array
    removeSelectedFromEventArray( bucketTree );

    // Remove selected items
    removeSelectedFromBucket( bucketTree );

    // Disable remove command
    updateCommands( gInviteRemoveCommandsArray, true );
}   // End function removeSelectedFromInviteBucket



/***** removeSelectedFromEventArray
* METHODS
*   removeSelectedFromEventArray:   removes the selected cards in the
*                                   invite list from the array of cards
*                                   to be attached to the event
*
* PARAMETERS
*   None
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function removeSelectedFromEventArray( bucketTree )
{
    if( bucketTree )
    {
        var body = document.getElementById( kRECIPIENT_LIST_TREE_CHILDREN );
        var selection = bucketTree.view.selection;
        var rangeCount = selection.getRangeCount();
        
        for( i = rangeCount-1; i >= 0; --i )
        {
            var start = {}, end = {};
            selection.getRangeAt( i, start, end );
            var item;
            var cardId;

            for( j = end.value; j >= start.value; --j )
            {
                item = bucketTree.contentView.getItemAtIndex( j );
                cardId = item.getAttribute( kINVITE_LIST_ITEM_ATTRIBUTE_CARD_ID );
                gEventCardArray[ cardId ] = null;
            }
        }
    }
}   // End function removeSelectedFromEventArray



/***** setupAttachedContacts
* METHODS
*   setupAttachedContacts:  display any existing attached contacts
*                           in the dialog    
*
* PARAMETERS
*   attachedContacts:   array of nsIAbCards
*
* RETURN VALUE
*   None
*
* NOTES
**********
*/
function setupAttachedContacts( attachedContacts )
{
    var address;

    for( var cardId in attachedContacts )
    {
        address = "" + getAddressFromCard( attachedContacts[ cardId ] );    
        addInviteAddressIntoBucket( address, cardId );
    }
}   // End function setupAttachedContacts

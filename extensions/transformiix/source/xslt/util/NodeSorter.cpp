/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is XSL:P XSLT processor.
 *
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco (C) 1999-2000 Keith Visco.
 * All Rights Reserved..
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * $Id: NodeSorter.cpp,v 1.2 2000/04/13 10:37:49 kvisco%ziplink.net Exp $
 */



#include "NodeSorter.h"

/*
 * Sorts Nodes as specified by the W3C XSLT 1.0 Recommendation
 */


//-- static variables
const String NodeSorter::DEFAULT_LANG     = "en";
const String NodeSorter::ASCENDING_ORDER  = "ascending";
const String NodeSorter::DESCENDING_ORDER = "descending";
const String NodeSorter::TEXT_TYPE        = "text";
const String NodeSorter::NUMBER_TYPE      = "number";

/**
 * Sorts the given Node Array using this XSLSort's properties as a
 * basis for the sort
 * @param nodes the Array of nodes to sort
 * @param pState the ProcessorState to evaluate the Select pattern with
 * @return the a new Array of sorted nodes
 **/
void NodeSorter::sort
    (NodeSet* nodes, Element* sortElement, Node* context, ProcessorState* ps)
{

    if ((!nodes) || (!sortElement)) return;


    // Build sortKeys table
    NamedMap keyHash;

    //-- create a new pointer to sortElement, we will use this
    //-- to handle multiple sort keys
    //-- (ie. xsl:sort siblings of sortElement)
    Element*  xslSort     = sortElement;


    String lang(DEFAULT_LANG);
    String order(ASCENDING_ORDER);
    String datatype(TEXT_TYPE);

    ExprParser exprParser;  //-- I'll make this static soon
    Expr* selectExpr = ps->getExpr(xslSort->getAttribute(SELECT_ATTR));

    //avt = exprParser.createAttributeValueTemplate(attValue);
    //ExprResult* exprResult = avt->evaluate(context,ps);
    //exprResult->stringValue(result);
    //delete exprResult;
    //delete avt;

    AttributeValueTemplate* avt = 0;
    ExprResult* exprResult = 0;

     // get lang  (Not Yet Implemented)
     // avt = xslSort.getAttributeAsAVT(XSLSort.LANG_ATTR);
     // if (avt) lang = avt.evaluate(context, pState);

     // Get Order
     DOMString attValue = xslSort->getAttribute(ORDER_ATTR);
     if (attValue.length() > 0) {
         avt = exprParser.createAttributeValueTemplate(attValue);
         exprResult = avt->evaluate(context, ps);
         order.clear();
         exprResult->stringValue(order);
         //-- clean up
         delete exprResult;
         delete avt;
         avt = 0;
      }


     // Get DataType (Not Yet Implemented)
     // avt = xslSort.getAttributeAsAVT(XSLSort.DATA_TYPE_ATTR);
     // if (avt) dataType = avt.evaluate(context, pState);

     MBool ascending = order.isEqual(ASCENDING_ORDER);

     NodeSet noKeyBucket;


     //-- fix, declare "i" here since some compiler's can't local
     //-- scope inside the for-declaration
     int i = 0;

     // Build hash table of sort keys
     for ( ; i < nodes->size(); i++) {
         Node* node = nodes->get(i);
         String* sortKey = new String();
         exprResult = selectExpr->evaluate(node, ps);
         if ((!exprResult) || (exprResult->getResultType() != ExprResult::NODESET)) {
             //-- should we flag this as an error?
             //String err("Error in select expr of xslt sort element, expecting NodeSet as result of: ");
             //selectExpr->toString(err);
             //ps->recieveError(err);
             //delete exprResult;
             //continue;
         }
         else {
             NodeSet* resultNodes = (NodeSet*)exprResult;
             if (resultNodes->size() > 0) {
                 XMLDOMUtils::getNodeValue(resultNodes->get(0), sortKey);
             }
         }

         if (sortKey->length() == 0) noKeyBucket.add(node);
         else {
             NodeSet* bucket = (NodeSet*) keyHash.get(*sortKey);
             if (!bucket) {
                 bucket = new NodeSet();
                 bucket->add(node);
                 keyHash.put(*sortKey, bucket);
             }
             else bucket->add(node);
         }

        delete exprResult;
    }

    // Return sorted Nodes
    StringList* list = keyHash.keys();
    int nbrKeys = list->getLength();
    String** keys = new String*[nbrKeys];
    StringListIterator* iter = list->iterator();
    int c = 0;
    while (iter->hasNext()) {
        keys[c++] = iter->next();
    }
    delete iter;

    //-- sort keys
    sortKeys(keys, nbrKeys, ascending, datatype, lang);

    nodes->clear();

    // add all nodes that had no Key (using document order)
    for (i = 0; i < noKeyBucket.size(); i++) {
       nodes->add(noKeyBucket.get(i));
    }

    // Add All nodes with sorted keys
    for (i = 0; i < nbrKeys; i++) {

       NodeSet* bucket = (NodeSet*) keyHash.remove(*keys[i]);

       //-- set keys[i] = 0 so that it doesn't get deleted twice
       //-- since deleting "list" will delete the keys
       keys[i] = 0;

       // Sort Buckets if necessary (Not Yet Implemented)
       //if (bucket.size() > 1) {
       //    bucket = sort(bucket,newSortElements,context,pState);
       //}
       int j;
       for (j = 0; j < bucket->size(); j++) {
           nodes->add(bucket->get(j));
       }
       delete bucket;
    }

    delete keys;
    //-- important to delete after we set keys[0..n] = 0;
    //-- since the list destructor will delete the String pointers
    delete list;

} //-- sort



/**
 * Sorts the given String[]
**/
void NodeSorter::sortKeys
    (String** keys, int length, MBool ascending, const String& dataType, const String& lang)
{

    if (length < 2) return;

    StringComparator* comparator = StringComparator::getInstance(lang);

    String** sorted = new String*[length];

    int nbrSorted = 0;

    int i = 0;

    while (nbrSorted < length) {

        int next = 0;
        //-- advance to next key
        while (!keys[next]) ++next;

        //-- find smallest
        for (i = next+1; i < length; i++) {

            if (!keys[i]) continue;

            if (ascending) {
                if (comparator->compare(*keys[i], *keys[next]) < 0) next = i;
            }
            else {
                if (comparator->compare(*keys[i], *keys[next]) > 0) next = i;
            }
        }

        sorted[nbrSorted++] = keys[next];
        keys[next] = 0;
    }

    //-- swap
    for (i = 0; i < length; i++) {
        keys[i] = sorted[i];
        sorted[i] = 0;
    }
    //-- clean
    delete comparator;
    delete [] sorted;

} //-- sortKeys

/**
 * Sorts the given String Array by converting the Strings to Numbers
 * and performing a comparison
 * @return the new sorted String Array
**
private static String[] sortAsNumbers
    (String[] strings, MBool ascending, String lang)
{
    if (strings.length == 0) return new String[0];

        List sorted = new List(strings.length);
        sorted.add(strings[0]);
        String key;
        int comp = -1;
        // Uses a simple insertion sort
        for (int i = 1; i < strings.length; i++) {
            key = strings[i];
            for (int j = 0; j < sorted.size(); j++) {
                comp = compareAsNumbers(key, (String)sorted.get(j));

                // Ascending
                if (ascending) {
                    if (comp < 0) {
                        sorted.add(j, key);
                        break;
                    }
                    else if (j == sorted.size()-1) {
                        sorted.add(key);
                        break;
                    }
                }
                // Descending
                else {
                    if (comp > 0) {
                        sorted.add(j, key);
                        break;
                    }
                    else if (j == sorted.size()-1) {
                        sorted.add(key);
                        break;
                    }
                }
            } //-- end for each sorted key
        }
        return (String[]) sorted.toArray(new String[sorted.size()]);

    } //-- sortAsNumbers


    private static int compareAsNumbers(String strA, String strB) {
        double dblA, dblB;

        try { dblA = (Double.valueOf(strA)).doubleValue(); }
        catch(NumberFormatException nfe) { dblA = 0; }
        try { dblB = (Double.valueOf(strB)).doubleValue(); }
        catch(NumberFormatException nfe) { dblB = 0; }

        if (dblA < dblB) return -1;
        else if (dblA == dblB) return 0;
        else return 1;
    } //-- compareAsNumbers

*/

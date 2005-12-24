/*
 * Entity.java
 *
 * Created on 01 October 2005, 12:04
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.query;

/**
 *
 * @author hash9
 */
interface Entity
{
  String makeSQL();
  String makeLDAP();
}

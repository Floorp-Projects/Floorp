/* -*- Mode: C#; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Manticore.
 *
 * The Initial Developer of the Original Code is
 * Silverstone Interactive.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

namespace Silverstone.Manticore.Core
{
  using System;
  using System.Collections;

  using Silverstone.Manticore.Bookmarks;
  
  public interface IDataStore 
  {
    void GetElements(String aContainerID, out IEnumerator aElements);
    
    void AddObserver(IDataStoreObserver aObserver);
  }

  /// <summary>
  /// Provides a means for a client to observe changes to a data store. 
  /// </summary>
  public interface IDataStoreObserver
  {
    void OnNodeChanging(Object aOldNode, Object aNewNode);
    void OnNodeChanged(Object aOldNode, Object aNewNode);

    void OnNodeAdding(Object aNewNode, Object aParentNode, int aIndex);
    void OnNodeAdded(Object aNewNode, Object aParentNode, int aIndex);

    void OnNodeRemoving(Object aNodeRemoving);
    void OnNodeRemoved(Object aNodeRemoved);
  }


  public class CommandTarget
  {
    public CommandTarget()
    {
    }
    
    public CommandTarget(String aLabel, String aAccessKey, Object aData, bool aIsContainer)
    {
      Label = aLabel;
      AccessKey = aAccessKey;
      Data = aData;
      IsContainer = aIsContainer;
    }
  
    private String mLabel = "";
    private String mAccesskey = "";
    private String mIconURL = "";
    private bool mIsContainer = false;
    private bool mIsOpen = false;
    private Object mData = new Object();

    public String Label 
    {
      get 
      {
        return mLabel;
      }
      set 
      {
        if (mLabel != value)
          mLabel = value;
      }
    }

    public String AccessKey
    {
      get 
      {
        return mAccesskey;
      }
      set 
      {
        if (mAccesskey != value) 
          mAccesskey = value;
      }
    }

    public bool IsContainer
    {
      get 
      {
        return mIsContainer;
      }
      set 
      {
        mIsContainer = value;
      }
    }

    public bool IsOpen
    {
      get 
      {
        return mIsOpen;
      }
      set 
      {
        mIsOpen = value;
      }
    }

    public String IconURL
    {
      get 
      {
        return mIconURL;
      }
      set 
      {
        mIconURL = value;
      }
    }

    public Object Data 
    {
      get 
      {
        return mData;
      }
      set 
      {
        if (!mData.Equals(value))
          mData = value;
      }
    }
  }

  public class DataStoreRegistry
  {
    public static IDataStore GetDataStore(String aDataStore)
    {
      switch (aDataStore) 
      {
        case "Bookmarks":
          // Need to replace this with a call to svc mgr.
          return ServiceManager.Bookmarks as IDataStore;
      }
      return null;
    }
  }

  public class SampleDataStore : IDataStore
  {
    private String mPrefix = "Goats";
    private Hashtable mTargets = null;
    private Hashtable mObservers = null;
  
    public SampleDataStore()
    {
      mTargets = new Hashtable();
      SetStuff();
    }
    
    public void AddObserver(IDataStoreObserver aObserver)
    {
      if (mObservers == null)
        mObservers = new Hashtable();
      
      mObservers.Add(aObserver.GetHashCode(), aObserver);
    }

    public void DoCommand(String aCommand, String aResourceID)
    {
    }
    
    private void SetStuff()
    {
      CommandTarget tgt1 = new CommandTarget(mPrefix + "1", "1", "foopy1" as Object, false);
      mTargets.Add(tgt1.GetHashCode(), tgt1);
      CommandTarget tgt2 = new CommandTarget(mPrefix + "2", "2", "foopy2" as Object, false);
      mTargets.Add(tgt2.GetHashCode(), tgt2);
      CommandTarget tgt3 = new CommandTarget(mPrefix + "3", "3", "foopy3" as Object, false);
      mTargets.Add(tgt3.GetHashCode(), tgt3);
      CommandTarget tgt4 = new CommandTarget(mPrefix + "4", "4", "foopy4" as Object, false);
      mTargets.Add(tgt4.GetHashCode(), tgt4);
    }
    
    public void ChangePrefix(String aPrefix)
    {
      /*
        CommandTarget[] oldTargets = new CommandTarget[4];
        mTargets.Values.CopyTo(oldTargets, 0);
        mPrefix = aPrefix;
        SetStuff();
        IEnumerator targets = mTargets.Values.GetEnumerator();
        int i = 0;
        while (targets.MoveNext()) {
          IEnumerator observers = mObservers.Values.GetEnumerator();
          while (observers.MoveNext()) {
            IDataStoreObserver idso = observers.Current as IDataStoreObserver;
            idso.OnNodeChanged(oldTargets[i++], targets.Current as CommandTarget);
          }
        }
        */
    }
    
    public void RemoveOdd()
    {
      IEnumerator targets = mTargets.Values.GetEnumerator();
      while (targets.MoveNext()) 
      {
        IEnumerator observers = mObservers.Values.GetEnumerator();
        while (observers.MoveNext()) 
        {
          IDataStoreObserver idso = observers.Current as IDataStoreObserver;
          CommandTarget current = targets.Current as CommandTarget;
          if (Int32.Parse(current.AccessKey) % 2 != 0) 
          {
            mTargets.Remove(current.Data.GetHashCode());
            idso.OnNodeRemoved(targets.Current as CommandTarget);
          }
        }
      }
    }
    
    public void AddItems()
    {
      CommandTarget tgt5 = new CommandTarget(mPrefix + "5", "5", "foopy5" as Object, false);
      mTargets.Add(tgt5.GetHashCode(), tgt5);
      CommandTarget tgt6 = new CommandTarget(mPrefix + "6", "6", "foopy6" as Object, false);
      mTargets.Add(tgt6.GetHashCode(), tgt6);
    }
    
    public void GetElements(String aContainerID, out IEnumerator aElements)
    {
      switch (aContainerID) 
      {
        case "root":
          aElements = mTargets.Values.GetEnumerator();
          break;
        case "goats":
          aElements = mTargets.Values.GetEnumerator();
          break;
        default:
          aElements = mTargets.Values.GetEnumerator();
          break;
      }
    }
  }

}
